// csapbcom.h
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#ifndef CSAPBCOM_H
#define CSAPBCOM_H

/**
 * \file
 *
 * \brief This header file is the entry point to CSAPBCOM functionality
 * and includes all necessary definitions.
 */

#ifdef __cplusplus
  #ifdef WIN32
    #ifdef CSAPBCOM_EXPORT_SYMBOLS
      #define CSAPBCOM_EXTERN extern "C" __declspec(dllexport)
    #else
      #define CSAPBCOM_EXTERN extern "C" __declspec(dllimport)
    #endif
  #else
    #define CSAPBCOM_EXTERN extern "C"
  #endif
#else
  #define CSAPBCOM_EXTERN
#endif

#include "stdint.h"
#include "stddef.h"

/**
 * \brief Connection information used to specifit the debug vehicle and SDC-600 COM port device
 */
CSAPBCOM_EXTERN typedef struct CSAPBCOMConnectionDescription
{
    const char* sdf;            /*!< SDF configuration file path */
    const char* address;        /*!< Connection address for the debug vehicle */
    int dapIndex;               /*!< Device index of the DAP for system reset */
    int deviceIndex;            /*!< Device index of the COM-AP or APBCOM device */
} CSAPBCOMConnectionDescription;

/**
 * \brief Return codes from CSAPBCOM_* functionality.
 *
 * These return codes delineate various responses from the function calls
 * featured in this API. All functions should return CSAPBCOM_SUCCESS if the
 * debug transaction is successful, otherwise the subsequent error codes
 * describe the error encountered.
 */
CSAPBCOM_EXTERN typedef enum CSAPBCOMReturnCode
{
    CSAPBCOM_SUCCESS              = 0x0000, /*!< Success, no error */
    CSAPBCOM_BADARG               = 0x0001, /*!< The client passed a bad argument to an CSAPBCOM function - typically a NULL pointer, a zero-length buffer or a zero device ID */
    CSAPBCOM_INVALID_HANDLE       = 0x0002, /*!< The session handle passed to the CSAPBCOM function was invalid */
    CSAPBCOM_FAILED               = 0x0003, /*!< Non-specific failure */
    CSAPBCOM_TOO_MANY_CONNECTIONS = 0x0004, /*!< All available connections are used. Only returned by {@link CSAPBCOM_Open} */
    CSAPBCOM_BUFFER_OVERFLOW      = 0x0005, /*!< Provided buffer is too small */
    CSAPBCOM_INTERNAL_ERROR       = 0x000D, /*!< An unspecified internal error occurred */
    CSAPBCOM_PARSE_FAILED         = 0x000E, /*!< {@link CSAPBCOM_Open} failed to parse the SDF file specified in {@link CSAPBCOMConnectionDescription} */
    CSAPBCOM_REG_ACCESS           = 0x0010, /*!< Register access failed */
    CSAPBCOM_TIMEOUT              = 0x0013, /*!< Operation timed out */
    CSAPBCOM_RW_FAIL              = 0x0016, /*!< Operation failed to access memory or a register for reasons not covered by any other reason */
    CSAPBCOM_DEV_UNKNOWN          = 0x002E, /*!< A command has been sent to a device that is not listed in the SDF file */
    CSAPBCOM_DEV_IN_USE           = 0x002F, /*!< Returned when the client tried to make an active connection to a device that already has an active connection */
    CSAPBCOM_NO_CONN              = 0x0030, /*!< No connection has been made to the specified vehicle. Ensure {@link CSAPBCOM_Connect} has been called successfully and the client has not disconnected.*/
    CSAPBCOM_COMMS                = 0x0032, /*!< General error returned when there is a problem with the communications channel */
    CSAPBCOM_DEV_BUSY             = 0x0038, /*!< The vehicle has been left in a busy state by a previous call or by another client */
    CSAPBCOM_NO_INIT              = 0x0039, /*!< No connection has been made to the specified device. Ensure {@link CSAPBCOM_Open} has been called successfully */
    CSAPBCOM_LOST_CONN            = 0x003A, /*!< The connection to the remote vehicle or device has been lost */
    CSAPBCOM_NO_VCC               = 0x003B, /*!< The device is not powered or has been disconnected */
    CSAPBCOM_NO_RESPONSE          = 0x0041, /*!< The requested operation failed because it timed out waiting for a response from the device */
    CSAPBCOM_OUT_OF_MEM           = 0x0043, /*!< Unable to allocate sufficient memory to complete the requested operation */
    CSAPBCOM_WRONG_DEV            = 0x0048, /*!< Device is not a SDC-600 COM port device. Either COM-AP or APBCOM */
    CSAPBCOM_NODEBUGPOWER         = 0x0057, /*!< The debug system on the target is not powered */
    CSAPBCOM_UNKNOWN              = 0x005A, /*!< An unknown error was encountered */
    CSAPBCOM_NO_CONFIG_FILE       = 0x0065, /*!< The SDF file supplied to in {@link CSAPBCOMConnectionDescription} to {@link CSAPBCOM_Open} cannot be found */
    CSAPBCOM_UNKNOWN_CONFIG       = 0x0068, /*!< An unknown SDF file error was encountered */
} CSAPBCOMReturnCode;

/**
 * \brief Reset parameters
 *
 */
CSAPBCOM_EXTERN typedef enum CSAPBCOMResetParams
{
    CSAPBCOM_RESET_BEGIN = 0x1, /**< Begin the classical nSRST sequence */
    CSAPBCOM_RESET_END = 0x2    /**< End the classical nSRST sequence and allow
                               the device to boot */
} CSAPBCOMResetParams;

/**
 * \brief Handle used to maintain state across multiple calls to CSAPBCOM_*
 * functionality.
 */
CSAPBCOM_EXTERN typedef int CSAPBCOMHandle;
static const int CSAPBCOMInvalidHandle = 0xFFFF;

/**
 * \brief Retrieve interface implementation version details
 *
 * Calling this function allows clients of the library to retrieve
 * specific implementation details before opening a connection to the library.
 *
 * This facilitates multiple code-paths depending on the returned version
 * and the context it implied. It is the responsibility of clients of this
 * library to interpret the version details correctly and act accordingly.
 *
 * \param[out] versionDetails Client supplied buffer to retrive an
 *                            implementation defined version string. The returned
 *                            string will be NULL terminated.
 *
 * \param[in] versionDetailsLength The size in bytes of the supplied
 *                                 versionDetails buffer. Note that a NULL
 *                                 terminating character will always be appended
 *                                 so clients must account for this extra size.
 *                                 Should an overflow be encountered the string
 *                                 will be truncated and an appropriate error
 *                                 message will be returned.
 */
CSAPBCOM_EXTERN CSAPBCOMReturnCode CSAPBCOM_GetInterfaceVersion(char* versionDetails,
        size_t versionDetailsLength);

/**
 * \brief Open a connection to the CSAPBCOM library and connect to the debug
 * vehicle.
 *
 * This is the initial function all clients must call when interacting with the
 * library. A unique handle will be returned and must be stored for subsequent
 * calls to the API.
 *
 * Internally, this function connects to the debug vehicle using using an address
 * and protocol specified in the topology argument.
 *
 * If the debug vehicle cannot be connected to, or an error occurs during the
 * process, a relevant error code will be returned.
 *
 * \param[out] outHandle Pointer to a handle. This function returns a unique
 * handle which identifies the current session and must be used in each of the
 * following calls to the library.
 *
 * \param[in] topology Implementation-specific data containing details regarding
 * the connection to the debug vehicle, and the target.
 */
CSAPBCOM_EXTERN CSAPBCOMReturnCode CSAPBCOM_Open(CSAPBCOMHandle* outHandle,
        const CSAPBCOMConnectionDescription* topology);

/**
 * \brief Close a connection to the debug vehicle and the CSAPBCOM library.
 *
 * \param[in] handle Handle previously retrieved by call to CSAPBCOM_Open()
 */
CSAPBCOM_EXTERN CSAPBCOMReturnCode CSAPBCOM_Close(CSAPBCOMHandle handle);

/**
 * \brief Connect to the target system's Debug Access Port.
 *
 * This function will not attempt to power-up the Debug Access Port.
 *
 * \param[in] handle Handle previously retrieved by call to CSAPBCOM_Open()
 */
CSAPBCOM_EXTERN CSAPBCOMReturnCode CSAPBCOM_Connect(CSAPBCOMHandle handle);

/**
 * \brief Disconnect from the target system's Debug Access Port.
 *
 * \param[in] handle Handle previously retrieved by call to CSAPBCOM_Open()
 */
CSAPBCOM_EXTERN CSAPBCOMReturnCode CSAPBCOM_Disconnect(CSAPBCOMHandle handle);

/**
 * \brief Read generic data from the APBCOM rxEngine.
 *
 * Data is read according to the RxEngine width so outData is not padded with
 * NULL bytes.
 *
 * \param[in] handle Handle previously retrieved by call to CSAPBCOM_Open()
 *
 * \param[in] numBytes The number of bytes to read
 *
 * \param[out] outData A client supplied buffer to receive the requested data
 *
 * \param[in] outDataLength The size of the client buffer in bytes. If this is
 *                          not large enough to receive the requested amount
 *                          of data then an error is returned
 */
CSAPBCOM_EXTERN CSAPBCOMReturnCode CSAPBCOM_ReadData(CSAPBCOMHandle handle,
        size_t numBytes, unsigned char* outData, size_t outDataLength);

/**
 * \brief Write generic data to the APBCOM TxEngine.
 *
 * Data is written according to the TxEngine width so there is no need to pad inData
 * with NULL bytes.
 *
 * Recommended for byte flags and small messages.
 *
 * \param[in] handle Handle previously retrieved by call to CSAPBCOM_Open()
 *
 * \param[in] block If zero no consideration is taken for the available space in the
 *                  TxEngine, it is therefore up to the caller to check
 *                  available space using {@link CSAPBCOM_GetStatus}. If data is
 *                  written without free space being available, an overflow will
 *                  occur.
 *                  If non-zero writes are blocked until there is sufficent space
 *                  in the TxEngine.
 *
 * \param[in] numBytes The number of bytes to write
 *
 * \param[in] inData A client supplied buffer to write the requested data to
 *                   the TxEngine
 */
CSAPBCOM_EXTERN CSAPBCOMReturnCode CSAPBCOM_WriteData(CSAPBCOMHandle handle,
        int block, size_t numBytes, const unsigned char* inData);

/**
 * \brief Perform a system and debug reset.
 *
 * \param[in] handle Handle previously retrieved by call to CSAPBCOM_Open()
 *
 * \param[in] resetType The reset type as enumerated in _CSAPBCOMResetParams
 */
CSAPBCOM_EXTERN CSAPBCOMReturnCode CSAPBCOM_SystemReset(CSAPBCOMHandle handle, CSAPBCOMResetParams resetType);

/**
* \brief Get various status values for the COM-AP or APBCOPM device.
*
* \param[in] handle Handle previously retrieved by call to CSAPBCOM_Open()
*
* \param[out] txFree Inidcates that the txEngine FIFO has at least the X bytes
* free for new data. Can be NULL.
*
* \param[out] txOverflow Indicates wheter the txEngine FIFO has overflown and therefore
* at least one byte has been written and lost. Can be NULL.
*
* \param[out] rxData Indicates if the rxEngine FIFO has at least X bytes to be consumed.
* Can be NULL.
*
* \param[out] linkErrs Indicates if any lik errors have been detected. Bit 0 - tx link error,
* bit 1 - rx link error, bits 2:7 - unused. Can be NULL.
*/
CSAPBCOM_EXTERN CSAPBCOMReturnCode CSAPBCOM_GetStatus(CSAPBCOMHandle handle, uint8_t * txFree, uint8_t * txOverflow,
        uint8_t * rxData, uint8_t * linkErrs);

#endif // CSAPBCOM_H
