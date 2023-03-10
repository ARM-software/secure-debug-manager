CMAKE_MINIMUM_REQUIRED (VERSION 3.1.0)

ADD_LIBRARY (secure_debug_manager SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/secure_debug_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/secure_debug_manager_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/psa_adac_crypto_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ext_com_port_driver.cpp
)

ADD_DEFINITIONS (-DSDM_EXPORT_SYMBOLS)

INCLUDE_DIRECTORIES (${CMAKE_CURRENT_SOURCE_DIR}
                     ${CMAKE_SOURCE_DIR}/depends/psa-adac/psa-adac/core/include
                     ${CMAKE_SOURCE_DIR}/depends/psa-adac/psa-adac/sdm/include
                     ${CMAKE_SOURCE_DIR}/depends/sdm-api/include
)

TARGET_LINK_LIBRARIES (secure_debug_manager PRIVATE mbedtls psa_adac_sdm)

IF (WIN32)
    INSTALL (TARGETS secure_debug_manager RUNTIME DESTINATION output)
    INSTALL (TARGETS secure_debug_manager RUNTIME DESTINATION arm_ds/DB/Boards/Arm/MPS3_Corstone-1000_ADAC/SDM)
ELSE ()
    INSTALL (TARGETS secure_debug_manager LIBRARY DESTINATION output)
    INSTALL (TARGETS secure_debug_manager LIBRARY DESTINATION arm_ds/DB/Boards/Arm/MPS3_Corstone-1000_ADAC/SDM)
ENDIF ()
