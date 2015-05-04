/*
    Copyright 2015 by Gregor Mi <codestruct@posteo.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef KMORETOOLS_P_H
#define KMORETOOLS_P_H

#include "kmoretools.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QTextCodec>
#include <QDebug>
#include <QDesktopServices>

#include <klocalizedstring.h>

#define _ QLatin1String

/**
 *
 */
class KmtMenuItemIdGen
{
public:
    QString getId(const QString& inputId)
    {
        int postFix = desktopEntryNameUsageMap[inputId];
        desktopEntryNameUsageMap[inputId] = postFix + 1;
        return QString(QLatin1String("%1%2")).arg(inputId).arg(postFix);
    }

    void reset()
    {
        desktopEntryNameUsageMap.clear();
    }

private:
    QMap<QString, int> desktopEntryNameUsageMap;
};

/**
 *
 */
class KmtMenuItemDto
{
public:
    QString id;

    /**
     * @note that is might contain an ampersand (&) which may be used for menu items.
     * Remove it with removeMenuAmpersand()
     */
    QString text;

    QIcon icon;

    KMoreTools::MenuSection menuSection;

    bool isInstalled = true;

    /**
     * only used if isInstalled == false
     */
    QUrl homepageUrl;

public:
    void jsonRead(const QJsonObject &json)
    {
        id = json[_("id")].toString();
        menuSection = json[_("menuSection")].toString() == _("main") ? KMoreTools::MenuSection_Main : KMoreTools::MenuSection_More;
        isInstalled = json[_("isInstalled")].toBool();
    }

    void jsonWrite(QJsonObject &json) const
    {
        json[_("id")] = id;
        json[_("menuSection")] = menuSection == KMoreTools::MenuSection_Main ? _("main") : _("more");
        json[_("isInstalled")] = isInstalled;
    }

    bool operator==(const KmtMenuItemDto rhs) const
    {
        return this->id == rhs.id;
    }

    /**
     * todo: is there a QT method that can be used insted of this?
     */
    static QString removeMenuAmpersand(const QString& str)
    {
        QString newStr = str;
        newStr.replace(QRegExp(_("\\&([^&])")), _("\\1")); // &Hallo --> Hallo
        newStr.replace(_("&&"), _("&")); // &&Hallo --> &Hallo
        return newStr;
    }
};

/**
 *
 */
class KmtMenuStructureDto
{
public:
    QList<KmtMenuItemDto> list;

public: // should be private but we would like to unit test

    /**
     * NOT USED
     */
    QList<const KmtMenuItemDto*> itemsBySection(KMoreTools::MenuSection menuSection) const
    {
        QList<const KmtMenuItemDto*> r;

        Q_FOREACH (const auto& item, list) {
            if (item.menuSection == menuSection) {
                r.append(&item);
            }
        }

        return r;
    }

    /**
     * don't store the returned pointer, but you can deref it which calls copy ctor
     */
    const KmtMenuItemDto* findInstalled(const QString& id) const {
        auto foundItem = std::find_if(list.begin(), list.end(),
        [id](const KmtMenuItemDto& item) {
            return item.id == id && item.isInstalled;
        });
        if (foundItem != list.end()) {
            // deref iterator which is a const MenuItemDto& from which we get the pointer
            // (todo: is this a good idea?)
            return &(*foundItem);
        }

        return nullptr;
    }

public:
    QString serialize() const
    {
        QJsonObject jObj;
        jsonWrite(jObj);
        QJsonDocument doc(jObj);
        auto jByteArray = doc.toJson(QJsonDocument::Compact);
        // http://stackoverflow.com/questions/14131127/qbytearray-to-qstring
        // QJsonDocument uses UTF-8 => we use 106=UTF-8
        //return QTextCodec::codecForMib(106)->toUnicode(jByteArray);
        return _(jByteArray); // accidently the ctor of QString takes an UTF-8 byte array
    }

    void deserialize(const QString& text)
    {
        QJsonParseError parseError;
        QJsonDocument doc(QJsonDocument::fromJson(text.toUtf8(), &parseError));
        jsonRead(doc.object());
    }

    void jsonRead(const QJsonObject &json)
    {
        list.clear();
        auto jArr = json[_("menuitemlist")].toArray();
        for (int i = 0; i < jArr.size(); ++i) {
            auto jObj = jArr[i].toObject();
            KmtMenuItemDto item;
            item.jsonRead(jObj);
            list.append(item);
        }
    }

    void jsonWrite(QJsonObject &json) const
    {
        QJsonArray jArr;
        Q_FOREACH (const auto item, list) {
            QJsonObject jObj;
            item.jsonWrite(jObj);
            jArr.append(jObj);
        }
        json[_("menuitemlist")] = jArr;
    }

    /**
     * @returns true if there are any not-installed items
     */
    std::vector<KmtMenuItemDto> notInstalledServices() const {
        std::vector<KmtMenuItemDto> target;
        std::copy_if(list.begin(), list.end(), std::back_inserter(target),
        [](const KmtMenuItemDto& item) {
            return !item.isInstalled;
        });
        return target;
    }

public: // should be private but we would like to unit test
    /**
     * stable sorts:
     * 1. main items
     * 2. more items
     * 3. not installed items
     */
    void stableSortListBySection()
    {
        std::stable_sort(list.begin(), list.end(), [](const KmtMenuItemDto& i1, const KmtMenuItemDto& i2) {
            return (i1.isInstalled && i1.menuSection == KMoreTools::MenuSection_Main && i2.isInstalled && i2.menuSection == KMoreTools::MenuSection_More)
                   || (i1.isInstalled && i1.menuSection == KMoreTools::MenuSection_More && !i2.isInstalled);
        });
    }

public:
    /**
    * moves an item up or down respecting its catgory
    * @param direction: 1: down, -1: up
    */
    void moveWithinSection(const QString& id, int direction)
    {
        auto selItem = std::find_if(list.begin(), list.end(),
        [id](const KmtMenuItemDto& item) {
            return item.id == id;
        });

        if (selItem != list.end()) { // if found
            if (direction == 1) { // "down"
                auto itemAfter = std::find_if(selItem + 1, list.end(), // find item where to insert after in the same category
                [selItem](const KmtMenuItemDto& item) {
                    return item.menuSection == selItem->menuSection;
                });

                if (itemAfter != list.end()) {
                    int prevIndex = list.indexOf(*selItem);
                    list.insert(list.indexOf(*itemAfter) + 1, *selItem);
                    list.removeAt(prevIndex);
                }
            } else if (direction == -1) { // "up"
                //auto r_list = list;
                //std::reverse(r_list.begin(), r_list.end()); // we need to search "up"
                //auto itemBefore = std::find_if(selItem, list.begin(),// find item where to insert before in the same category
                //                               [selItem](const MenuItemDto& item) { return item.menuSection == selItem->menuSection; });

                // todo: can't std::find_if be used instead of this loop?
                QList<KmtMenuItemDto>::iterator itemBefore = list.end();
                auto it = selItem;
                while(it != list.begin()) {
                    --it;
                    if (it->menuSection == selItem->menuSection) {
                        itemBefore = it;
                        break;
                    }
                }

                if (itemBefore != list.end()) {
                    int prevIndex = list.indexOf(*selItem);
                    list.insert(itemBefore, *selItem);
                    list.removeAt(prevIndex + 1);
                }
            } else {
                Q_ASSERT(false);
            }
        } else {
            qWarning() << "selItem != list.end() == false";
        }

        stableSortListBySection();
    }

    void moveToOtherSection(const QString& id)
    {
        auto selItem = std::find_if(list.begin(), list.end(),
                                    [id](const KmtMenuItemDto& item) -> bool { return item.id == id; });

        if (selItem != list.end()) { // if found
            if (selItem->menuSection == KMoreTools::MenuSection_Main) {
                selItem->menuSection = KMoreTools::MenuSection_More;
            } else if (selItem->menuSection == KMoreTools::MenuSection_More) {
                selItem->menuSection = KMoreTools::MenuSection_Main;
            } else {
                Q_ASSERT(false);
            }
        }

        stableSortListBySection();
    }
};

/**
 * In menu structure consisting of main section items, more section items
 * and registered services which are not installed
 */
class KmtMenuStructure
{
public:
    QList<KMoreToolsMenuItem*> mainItems;
    QList<KMoreToolsMenuItem*> moreItems;

    /**
     * contains each not installed registered service once
     */
    QList<KMoreToolsService*> notInstalledServices;

public:
    KmtMenuStructureDto toDto()
    {
        KmtMenuStructureDto result;

        Q_FOREACH (auto item, mainItems) {
            const auto a = item->action();
            KmtMenuItemDto dto;
            dto.id = item->id();
            dto.text = a->text(); // might be overriden, so we use directly from QAction
            dto.icon = a->icon();
            dto.isInstalled = true;
            dto.menuSection = KMoreTools::MenuSection_Main;
            result.list << dto;
        }

        Q_FOREACH (auto item, moreItems) {
            const auto a = item->action();
            KmtMenuItemDto dto;
            dto.id = item->id();
            dto.text = a->text(); // might be overriden, so we use directly from QAction
            dto.icon = a->icon();
            dto.isInstalled = true;
            dto.menuSection = KMoreTools::MenuSection_More;
            result.list << dto;
        }

        Q_FOREACH (auto registeredService, notInstalledServices) {
            KmtMenuItemDto dto;
            //dto.id = item->id(); // not used in this case
            dto.text = registeredService->formatString(_("$Name"));
            dto.icon = registeredService->icon();
            dto.isInstalled = false;
            // dto.menuSection = // not used in this case
            dto.homepageUrl = registeredService->homepageUrl();
            result.list << dto;
        }

        return result;
    }
};

class KmtNotInstalledUtil
{
public:
    static QMenu* createSubmenuForNotInstalledApp(const QString& title, QWidget* parent, const QIcon& icon, const QUrl& homepageUrl)
    {
        QMenu* submenuForNotInstalled = new QMenu(title, parent);
        submenuForNotInstalled->setIcon(icon);

        if (homepageUrl.isValid()) {
            auto websiteAction = submenuForNotInstalled->addAction(i18nc("@action:inmenu", "Visit homepage"));
            auto url = homepageUrl;
            // todo/review: is it ok to have sender and receiver the same object?
            QObject::connect(websiteAction, &QAction::triggered, websiteAction, [url](bool) {
                QDesktopServices::openUrl(url);
            });
        } else {
            submenuForNotInstalled->addAction(i18nc("@action:inmenu", "No further information available."))
            ->setEnabled(false);
        }

        return submenuForNotInstalled;
    }
};

#endif
