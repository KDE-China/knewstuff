include(ECMMarkAsTest)
include(ECMMarkNonGuiExecutable)
include(ECMAddTests)

find_package(Qt5Test ${REQUIRED_QT_VERSION} NO_MODULE REQUIRED)
set_package_properties(Qt5Test PROPERTIES
    TYPE REQUIRED
    PURPOSE "Required for unit tests")

# src also removes -DQT_NO_CAST_FROM_ASCII
remove_definitions(-DQT_NO_CAST_FROM_ASCII)
include_directories( ${CMAKE_CURRENT_BINARY_DIR}/../src ../src )

macro(knewstuff_unit_tests)
    foreach(_testname ${ARGN})
       ecm_add_test(${_testname}.cpp ../src/core/author.cpp ../src/core/entryinternal.cpp ../src/entry.cpp ../src/core/xmlloader.cpp ../src/core/jobs/httpjob.cpp ../src/core/jobs/httpworker.cpp  ../src/knewstuff_debug.cpp  ../src/core/knewstuffcore_debug.cpp
                    LINK_LIBRARIES Qt5::Xml Qt5::Test Qt5::Gui KF5::KIOCore KF5::NewStuffCore
                    TEST_NAME ${_testname}
                    NAME_PREFIX "knewstuff-")
       set_property( TARGET ${_testname}
                     APPEND
                     PROPERTY COMPILE_DEFINITIONS KNEWSTUFF_STATIC_DEFINE KNEWSTUFFCORE_STATIC_DEFINE KNEWSTUFF_STATIC_DEFINE
       )
    endforeach()
endmacro()

knewstuff_unit_tests(
    knewstuffauthortest
    knewstuffentrytest
)

# KMoreTools:
ecm_add_test(kmoretools/kmoretoolstest.cpp ../src/knewstuff_debug.cpp
             LINK_LIBRARIES Qt5::Test Qt5::Widgets KF5::NewStuffCore KF5::NewStuff KF5::I18n
             TEST_NAME kmoretoolstest)

# ===========

add_executable(kmoretoolstest2 kmoretools/kmoretoolstest2.cpp ../src/knewstuff_debug.cpp)
# The next line is commented out to prevent CI server to pick up those tests:
# add_test(kmoretoolstest2 kmoretoolstest2) # uncomment to run tests locally
ecm_mark_as_test(kmoretoolstest2)
target_link_libraries(kmoretoolstest2 Qt5::Test KF5::NewStuff KF5::I18n)

