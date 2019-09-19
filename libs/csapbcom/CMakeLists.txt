CMAKE_MINIMUM_REQUIRED (VERSION 3.1.0)
PROJECT (csapbcom)

IF (BUILD_TESTS)
    IF (NOT GOOGLETEST_ROOT)
        MESSAGE (FATAL_ERROR "GOOGLETEST_ROOT is not set!")
    ENDIF ()
ENDIF ()

IF(WIN32)
    SET (DEPENDS_LIB_PATH ${CMAKE_SOURCE_DIR}/depends/win_32-x86_64)
ELSE()
    SET (DEPENDS_LIB_PATH ${CMAKE_SOURCE_DIR}/depends/linux-x86_64)
ENDIF()

## Library
SET (CXX_SOURCE 
    src/csapbcom_interface.cpp)
SET (CXX_IMPL_SOURCE
    src/rddiwrapper/rddi_wrapper_impl_default.cpp)
INCLUDE_DIRECTORIES (
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/../RDDI_API/public/include)
ADD_LIBRARY (csapbcom SHARED ${CXX_SOURCE} ${CXX_IMPL_SOURCE})
ADD_DEFINITIONS(-DCSAPBCOM_EXPORT_SYMBOLS)

IF (WIN32)
    FIND_LIBRARY (RDDI_DEBUG_RVI rddi-debug-rvi_2.lib PATHS ${DEPENDS_LIB_PATH})
ELSE ()
    FIND_LIBRARY (RDDI_DEBUG_RVI librddi-debug-rvi.so.2 PATHS ${DEPENDS_LIB_PATH})
ENDIF ()

TARGET_LINK_LIBRARIES (csapbcom ${RDDI_DEBUG_RVI})
SET_TARGET_PROPERTIES (csapbcom PROPERTIES POSITION_INDEPENDENT_CODE ON)
SET_TARGET_PROPERTIES (csapbcom PROPERTIES INSTALL_RPATH "." CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

IF (WIN32)
    FILE (GLOB BOOST_LIBS ${DEPENDS_LIB_PATH}/boost_*.dll)
    INSTALL (TARGETS csapbcom RUNTIME DESTINATION output)
    INSTALL (FILES ${DEPENDS_LIB_PATH}/rddi-debug-rvi_2.dll DESTINATION output)
    INSTALL (FILES ${DEPENDS_LIB_PATH}/rvicomms.dll DESTINATION output)
    INSTALL (FILES ${DEPENDS_LIB_PATH}/xerces-c_3_1.dll DESTINATION output)
    INSTALL (TARGETS csapbcom RUNTIME DESTINATION arm_ds)
ELSE ()
    FILE (GLOB BOOST_LIBS ${DEPENDS_LIB_PATH}/libboost_*.so*)
    INSTALL (TARGETS csapbcom LIBRARY DESTINATION output)
    INSTALL (FILES ${DEPENDS_LIB_PATH}/librddi-debug-rvi.so.2 DESTINATION output)
    INSTALL (FILES ${DEPENDS_LIB_PATH}/librvicomms.so DESTINATION output)
    INSTALL (FILES ${DEPENDS_LIB_PATH}/libxerces-c-3.1.so DESTINATION output)
    INSTALL (TARGETS csapbcom LIBRARY DESTINATION arm_ds)
ENDIF ()

INSTALL (FILES ${BOOST_LIBS} DESTINATION output)

## Tests
IF (BUILD_TESTS)
    IF (UNIX)
        SET (CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -pthread)
    ENDIF ()
    INCLUDE_DIRECTORIES (
        ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/googletest/include
        ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/googlemock/include
        ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/googletest
        ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/googlemock)

    GET_FILENAME_COMPONENT (GTEST_SRC_ROOT
        ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT} ABSOLUTE)

    SET (GTEST_SOURCE
        ${GTEST_SRC_ROOT}/googletest/src/gtest-all.cc
        ${GTEST_SRC_ROOT}/googletest/src/gtest_main.cc
        ${GTEST_SRC_ROOT}/googlemock/src/gmock-all.cc)

    SET (CXX_UNITTEST_SOURCE
        tests/csapbcom_interface_test.cpp)

    SET (CXX_TEST_STUBS
        src/rddiwrapper/rddi_wrapper_impl_stubbed.cpp)

    ADD_LIBRARY (csapbcom_test_lib ${CXX_SOURCE} ${CXX_TEST_STUBS})
    ADD_EXECUTABLE (csapbcom_unittests ${GTEST_SOURCE} ${CXX_UNITTEST_SOURCE})
    TARGET_LINK_LIBRARIES (csapbcom_unittests csapbcom_test_lib)
ENDIF ()
