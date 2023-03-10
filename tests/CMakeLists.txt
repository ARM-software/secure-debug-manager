CMAKE_MINIMUM_REQUIRED (VERSION 3.1.0)

IF (NOT GOOGLETEST_ROOT)
    MESSAGE (FATAL_ERROR "GOOGLETEST_ROOT is not set!")
ENDIF ()

IF (UNIX)
    SET (CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -pthread)
ENDIF ()

INCLUDE_DIRECTORIES (
    ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/googletest/include
    ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/googlemock/include
    ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/googletest
    ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/googlemock
    ${CMAKE_SOURCE_DIR}/depends/sdm-api/include
    ${CMAKE_SOURCE_DIR}/depends/psa-adac/psa-adac/core/include
    ${CMAKE_SOURCE_DIR}/sdm)

GET_FILENAME_COMPONENT (GTEST_SRC_ROOT
    ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT} ABSOLUTE)

SET (GTEST_SOURCE
    ${GTEST_SRC_ROOT}/googletest/src/gtest-all.cc
    ${GTEST_SRC_ROOT}/googletest/src/gtest_main.cc
    ${GTEST_SRC_ROOT}/googlemock/src/gmock-all.cc)

SET (CXX_SOURCE
    ${CMAKE_SOURCE_DIR}/sdm/ext_com_port_driver.cpp)

SET (CXX_UNITTEST_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/ext_com_port_driver_test.cpp)

ADD_EXECUTABLE (ext_com_port_driver_unittests ${GTEST_SOURCE} ${CXX_SOURCE} ${CXX_UNITTEST_SOURCE})
