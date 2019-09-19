// sdc600_log.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#ifndef SDC600_LOG_H_
#define SDC600_LOG_H_

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define __func__ __FUNCTION__
#endif

#define SDC600_LOG_PRINT printf

#define SDC600_LOG_LVL_ERROR   30
#define SDC600_LOG_LVL_WARN    20
#define SDC600_LOG_LVL_INFO    10
#define SDC600_LOG_LVL_DEBUG   0


#define SDC600_WAIT(_a) \
    do { \
        uint32_t _cnt = 0; \
        do { \
            _cnt++; \
        } while(_a > _cnt); \
    } while(0)

#define SDC600_LOG_LEVEL   SDC600_LOG_LVL_DEBUG

#define SDC600_TRACE           1

#define SDC600_LOG_FUNC_AND_LEVEL(_level, _who) \
                    SDC600_LOG_PRINT("%-40.40s:% 5d : %-5.5s : %-10.10s : ", __func__, __LINE__, _level, _who)

#define SDC600_LOG_PRINT_LINE(_who, _level, _format, ...) \
    printf("%-40.40s:% 5d : %-5.5s : %-10.10s : "_format, __func__, __LINE__, _who, _level, ##__VA_ARGS__)

#if SDC600_LOG_LEVEL <= SDC600_LOG_LVL_ERROR
#define SDC600_LOG_ERR(_who, _format, ...) \
    SDC600_LOG_PRINT_LINE("error", _who, _format,  ##__VA_ARGS__)
#else
#define SDC600_LOG_ERR(_who, _format, ...) \
    do{} while(0)
#endif

#if SDC600_LOG_LEVEL <= SDC600_LOG_LVL_WARN
#define SDC600_LOG_WARN(_who, _format, ...) \
    SDC600_LOG_PRINT_LINE("warn", _who, _format,  ##__VA_ARGS__)
#else
#define SDC600_LOG_WARN(_who, _format, ...) \
    do{} while(0)
#endif

#if SDC600_LOG_LEVEL <= SDC600_LOG_LVL_INFO
#define SDC600_LOG_INFO(_who, _format, ...) \
    SDC600_LOG_PRINT_LINE("info", _who, _format,  ##__VA_ARGS__)
#else
#define SDC600_LOG_INFO(_who, _format, ...) \
    do{} while(0)
#endif

#if SDC600_LOG_LEVEL <= SDC600_LOG_LVL_DEBUG
#define SDC600_LOG_DEBUG(_who, _format, ...) \
    SDC600_LOG_PRINT_LINE("debug", _who, _format,  ##__VA_ARGS__)
#else
#define SDC600_LOG_DEBUG(_who, _format, ...) \
    do{} while(0)
#endif

#define SDC600_LOG_BUF(_who, _buff, _size, _label)  \
                do {\
                    uint32_t i = 0, j = 0;\
                    for (i = 0; i * 16 + j < _size; i++, j = 0)\
                    { \
                        char tmpBuff[256] = {0}; \
                        for (j = 0; i * 16 + j < _size && j < 16; j++) \
                        { \
                            sprintf(tmpBuff + strlen(tmpBuff), "%02x", ((uint8_t*)_buff)[i * 16 + j]); \
                        } \
                        SDC600_LOG_FUNC_AND_LEVEL("debug", _who); \
                        SDC600_LOG_PRINT("%-10.10s %04x: %s\n", _label, i * 16, tmpBuff); \
                    } \
                } while(0)


#define SDC600_ASSERT_ERROR(_cmd, _exp, _error) \
                do { \
                    int _res = 0; \
                    if (SDC600_TRACE) SDC600_LOG_DEBUG(ENTITY_NAME, "running[%s]\n", #_cmd); \
                    if ((_res = (int)(_cmd)) != _exp) \
                    { \
                        SDC600_LOG_ERR(ENTITY_NAME, "failed to run[%s] res[%d] returning[%s]\n", #_cmd, _res, #_error); \
                        res = _error; \
                        goto bail; \
                    } \
                } while (0)

#define SDC600_ASSERT(_cmd, _exp) \
                do { \
                    if (SDC600_TRACE) SDC600_LOG_DEBUG(ENTITY_NAME, "running[%s]\n", #_cmd); \
                    if ((res = (_cmd)) != _exp) \
                    { \
                        SDC600_LOG_ERR(ENTITY_NAME, "failed to run[%s] res[%d]\n", #_cmd, res); \
                        goto bail; \
                    } \
                } while (0)
#endif /* SDC600_LOG_H_ */
