CMAKE_MINIMUM_REQUIRED (VERSION 3.1.0)
PROJECT (secure_debug_manager)

ADD_LIBRARY (secure_debug_manager SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/secure_debug_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/secure_debug_protocol.cpp
)

ADD_DEFINITIONS (-DSDM_EXPORT_SYMBOLS)

INCLUDE_DIRECTORIES (${DEPENDS_INC_PATH})

TARGET_LINK_LIBRARIES (secure_debug_manager auth_token_provider ext_com_port csapbcom)
IF (WIN32)
    INSTALL (TARGETS secure_debug_manager RUNTIME DESTINATION output)
    INSTALL (TARGETS secure_debug_manager RUNTIME DESTINATION arm_ds)
ELSE ()
    INSTALL (TARGETS secure_debug_manager LIBRARY DESTINATION output)
    INSTALL (TARGETS secure_debug_manager LIBRARY DESTINATION arm_ds)
ENDIF ()
