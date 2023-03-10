CMAKE_MINIMUM_REQUIRED (VERSION 3.1.0)

IF (WIN32)
    SET (RDDI_LIB_PATH ${CMAKE_SOURCE_DIR}/depends/RDDI/libs/win_32-x86_64)
    FIND_LIBRARY (RDDI_DEBUG_RVI rddi-debug-rvi_2.lib PATHS ${RDDI_LIB_PATH})
ELSE ()
    SET (RDDI_LIB_PATH ${CMAKE_SOURCE_DIR}/depends/RDDI/libs/linux-x86_64)
    FIND_LIBRARY (RDDI_DEBUG_RVI librddi-debug-rvi.so.2 PATHS ${RDDI_LIB_PATH})
ENDIF ()

INCLUDE_DIRECTORIES (
    ${CMAKE_SOURCE_DIR}/depends/sdm-api/include
    ${CMAKE_SOURCE_DIR}/depends/RDDI/include
    ${CMAKE_SOURCE_DIR}/sdm)

ADD_EXECUTABLE (debugger_example
    ${CMAKE_SOURCE_DIR}/example/main.cpp
)
TARGET_LINK_LIBRARIES (debugger_example PRIVATE secure_debug_manager ${RDDI_DEBUG_RVI})

INSTALL (TARGETS debugger_example RUNTIME DESTINATION output)

IF (WIN32)
    INSTALL (FILES ${RDDI_LIB_PATH}/rddi-debug-rvi_2.dll DESTINATION output)
    INSTALL (FILES ${RDDI_LIB_PATH}/rvicomms.dll DESTINATION output)
    INSTALL (FILES ${RDDI_LIB_PATH}/xerces-c_3_1.dll DESTINATION output)
ELSE ()
    INSTALL (FILES ${RDDI_LIB_PATH}/librddi-debug-rvi.so.2 DESTINATION output)
    INSTALL (FILES ${RDDI_LIB_PATH}/librvicomms.so DESTINATION output)
    INSTALL (FILES ${RDDI_LIB_PATH}/libxerces-c-3.1.so DESTINATION output)
ENDIF ()
