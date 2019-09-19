// secure_debug_types.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#ifndef COMMON_SECURE_DEBUG_TYPES_H_
#define COMMON_SECURE_DEBUG_TYPES_H_

#include <stdint.h>

#ifdef _WIN32
#ifdef SDM_EXPORT_SYMBOLS
#define SDM_EXTERN __declspec(dllexport)
#else
#define SDM_EXTERN __declspec(dllimport)
#endif
#else
#define SDM_EXTERN
#endif

#define SD_RESPONSE_LENGTH              6

SDM_EXTERN typedef uint8_t sd_id_response_buffer_t[SD_RESPONSE_LENGTH];

#endif /* COMMON_SECURE_DEBUG_TYPES_H_ */
