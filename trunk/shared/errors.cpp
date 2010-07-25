/***************************************************************************
*
* File:     errors.cpp
* Content:  Functions used to map various types of numeric error code to
*           human-readable error names and descriptions.
*
***************************************************************************/

#include "errors.h"

#include <winerror.h>           // For basic error codes
#include <strsafe.h>            // For StringCchCopyA etc.

// NOTE: The header files that define the error codes we report below must
// be included before errors.cpp.  We do not include them here to avoid
// adding dependencies to all our clients.  If a given header file has not
// been included, the error codes it defines will not be reported correctly.
//
// Examples of header files that you might to include:
//
// [FIXME]

// Longest strings returned by ComponentErrorName and ComponentErrorDescription
#define MAX_ERROR_NAME_SIZE 64
#define MAX_ERROR_DESCRIPTION_SIZE 256

// Define countof() if necessary (better version at http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx)
#ifndef countof
    #define countof(a) (sizeof(a) / sizeof *(a))
#endif

// Name our tables according to the component name prefix to avoid conflicts
#define g_facilityTable _fix(g_) ## _fix(TRACE_COMPONENT) ## FacilityTable
#define g_hrTable       _fix(g_) ## _fix(TRACE_COMPONENT) ## HrTable


/***************************************************************************
 *
 * Error and facility code tables
 *
 ***************************************************************************/

#define HRDETAILS(hr, description) {hr, #hr, description}
#define WINERRDETAILS(errcode, description) {__HRESULT_FROM_WIN32(errcode), #errcode, description}

struct ErrorEntryDetails
{
    HRESULT hrCode;
    LPCSTR szName;
    LPCSTR szDescription;
};

// FIXME: obsolete
ErrorEntryDetails g_facilityTable[] =
{
    {0xABCD0000, "Foo"},  // FIXME: Old ones were mostly audio-specific
    {0x00040000, "interface-specific"}
};

ErrorEntryDetails g_hrTable[] =
{
    // Success codes
    HRDETAILS(S_OK,                                     "Success"),
    HRDETAILS(S_FALSE,                                  "Qualified success (see function documentation)"),

    // General error codes
    HRDETAILS(E_ACCESSDENIED,                           "Generic access denied error"),
    HRDETAILS(E_OUTOFMEMORY,                            "Out of memory"),
    HRDETAILS(E_INVALIDARG,                             "Invalid arguments"),
    HRDETAILS(E_UNEXPECTED,                             "Catastrophic failure"),
    HRDETAILS(E_NOTIMPL,                                "Not implemented"),
    HRDETAILS(E_POINTER,                                "Invalid pointer"),
    HRDETAILS(E_HANDLE,                                 "Invalid handle"),
    HRDETAILS(E_ABORT,                                  "Operation aborted"),
    HRDETAILS(E_FAIL,                                   "Generic unspecified error"),

    // Win32 error codes
    WINERRDETAILS(ERROR_INVALID_FUNCTION,               "Incorrect function"),
    WINERRDETAILS(ERROR_NOT_FOUND,                      "Element not found"),
    WINERRDETAILS(ERROR_FILE_NOT_FOUND,                 "The system cannot find the file specified"),
    WINERRDETAILS(ERROR_PATH_NOT_FOUND,                 "The system cannot find the path specified"),
    WINERRDETAILS(ERROR_ACCESS_DENIED,                  "Access is denied"),
    WINERRDETAILS(ERROR_INVALID_HANDLE,                 "The handle is invalid"),
    WINERRDETAILS(ERROR_OUTOFMEMORY,                    "Not enough storage is available to complete this operation"),
    WINERRDETAILS(ERROR_NOT_ENOUGH_MEMORY,              "Not enough storage is available to process this command"),
    WINERRDETAILS(ERROR_INVALID_DATA,                   "The data is invalid"),
    WINERRDETAILS(ERROR_NO_MORE_FILES,                  "There are no more files"),
    WINERRDETAILS(ERROR_GEN_FAILURE,                    "A device attached to the system is not functioning"),
    WINERRDETAILS(ERROR_SHARING_VIOLATION,              "The process cannot access the file because it is being used by another process"),
    WINERRDETAILS(ERROR_LOCK_VIOLATION,                 "The process cannot access the file because another process has locked a portion of the file"),
    WINERRDETAILS(ERROR_HANDLE_DISK_FULL,               "The disk is full"),
    WINERRDETAILS(ERROR_NOT_SUPPORTED,                  "The request is not supported"),
    WINERRDETAILS(ERROR_FILE_EXISTS,                    "The file exists"),
    WINERRDETAILS(ERROR_CANNOT_MAKE,                    "The directory or file cannot be created"),
    WINERRDETAILS(ERROR_INVALID_PARAMETER,              "The parameter is incorrect"),
    WINERRDETAILS(ERROR_OPEN_FAILED,                    "The system cannot open the device or file specified"),
    WINERRDETAILS(ERROR_BUFFER_OVERFLOW,                "The file name is too long"),
    WINERRDETAILS(ERROR_DISK_FULL,                      "There is not enough space on the disk"),
    WINERRDETAILS(ERROR_CALL_NOT_IMPLEMENTED,           "This function is not supported on this system"),
    WINERRDETAILS(ERROR_INVALID_NAME,                   "The filename, directory name, or volume label syntax is incorrect"),
    WINERRDETAILS(ERROR_MOD_NOT_FOUND,                  "The specified module could not be found"),
    WINERRDETAILS(ERROR_PROC_NOT_FOUND,                 "The specified procedure could not be found"),
    WINERRDETAILS(ERROR_NEGATIVE_SEEK,                  "An attempt was made to move the file pointer before the beginning of the file"),
    WINERRDETAILS(ERROR_BAD_ARGUMENTS,                  "One or more arguments are not correct"),
    WINERRDETAILS(ERROR_BAD_PATHNAME,                   "The specified path is invalid"),
    WINERRDETAILS(ERROR_BUSY,                           "The requested resource is in use"),
    WINERRDETAILS(ERROR_ALREADY_EXISTS,                 "Cannot create a file when that file already exists"),
    WINERRDETAILS(ERROR_INVALID_FLAG_NUMBER,            "The flag passed is not correct"),
    WINERRDETAILS(ERROR_MORE_DATA,                      "More data is available"),
    WINERRDETAILS(ERROR_NO_MORE_ITEMS,                  "No more data is available"),
    WINERRDETAILS(ERROR_MAX_THRDS_REACHED,              "No more threads can be created"),
    WINERRDETAILS(ERROR_NO_SYSTEM_RESOURCES,            "Out of memory or some other system resource"),
    WINERRDETAILS(WAIT_TIMEOUT,                         "The wait operation timed out"),
    WINERRDETAILS(WAIT_ABANDONED,                       "The resource being waited for has been destroyed"),
    WINERRDETAILS(RPC_S_UNKNOWN_IF,                     "Attempt to use an unknown RPC interface"),
    WINERRDETAILS(RPC_S_CALL_FAILED,                    "A remote procedure call failed"),

    // COM error codes
    #ifdef REGDB_E_CLASSNOTREG
        #pragma message("Compiling COM error codes...")
        HRDETAILS(CO_E_NOTINITIALIZED,                  "CoInitialize has not been called"),
        HRDETAILS(CO_E_ALREADYINITIALIZED,              "CoInitialize has already been called"),
        HRDETAILS(REGDB_E_CLASSNOTREG,                  "Class not registered"),
        HRDETAILS(E_NOINTERFACE,                        "No such interface supported"),
    #endif

    // FIXME: Add Unix errors?
};


/***************************************************************************
 *
 *  [Component]ErrorName
 *
 *  Description:
 *      Translates a variety of HRESULT codes to human-readable string names.
 *      Note that the returned string must be used before ErrorName is called
 *      again on any thread.
 *
 *  Arguments:
 *      HRESULT [in]: result code.
 *
 *  Returns:
 *      LPCSTR: error name.
 *
 ***************************************************************************/

LPCSTR ComponentErrorName(HRESULT hr)
{
    static char szErrorName[MAX_ERROR_NAME_SIZE];

    ComponentErrorNameAndDescription(hr, szErrorName, countof(szErrorName), NULL, 0);

    return szErrorName;
}


/***************************************************************************
 *
 *  [Component]ErrorDescription
 *
 *  Description:
 *      Translates a variety of HRESULT codes to human-readable descriptions.
 *      Note that the returned string must be used before ErrorDescription is
 *      called again on any thread.
 *
 *  Arguments:
 *      HRESULT [in]: result code.
 *
 *  Returns:
 *      LPCSTR: error description.
 *
 ***************************************************************************/

LPCSTR ComponentErrorDescription(HRESULT hr)
{
    static char szErrorDesc[MAX_ERROR_DESCRIPTION_SIZE];

    ComponentErrorNameAndDescription(hr, NULL, 0, szErrorDesc, countof(szErrorDesc));

    return szErrorDesc;
}


/***************************************************************************
 *
 *  [Component]ErrorNameAndDescription
 *
 *  Description:
 *      Translates a variety of HRESULT codes to textual names and descriptions.
 *
 *  Arguments:
 *      HRESULT [in]: result code.
 *      LPSTR [out]: string pointer to receive error name string.
 *      size_t [in]: size of above buffer in characters.
 *      LPSTR [out]: string pointer to receive error name description.
 *      size_t [in]: size of above buffer in characters.
 *
 *  Returns:
 *      HRESULT : S_OK on success, or STRSAFE_E_INSUFFICIENT_BUFFER if one of the
 *                buffers wasn't large enough to store the HRESULT information.
 *
 ***************************************************************************/

HRESULT ComponentErrorNameAndDescription
(
    HRESULT hrCode,
    __out_ecount_opt(cchName) LPSTR pszName, size_t cchName,
    __out_ecount_opt(cchDesc) LPSTR pszDesc, size_t cchDesc
)
{
    HRESULT hr = S_OK;
    int i;

    // Look for the error code in our error table
    for (i = 0; i < countof(g_hrTable); ++i)
    {
        if (hrCode == g_hrTable[i].hrCode)
        {
            if (pszName)
            {
                hr = StringCchCopyA(pszName, cchName, g_hrTable[i].szName);
            }
            if (SUCCEEDED(hr) && pszDesc)
            {
                hr = StringCchCopyA(pszDesc, cchDesc, g_hrTable[i].szDescription);
            }
            return hr;
        }
    }

    // If the specific error is not in our table, look for its facility code instead
    if (pszName)
    {
        HRESULT hrFacilityCode = hrCode & 0x7FFF0000;
        for (i = 0; i < countof(g_facilityTable); ++i)
        {
            if (hrFacilityCode == g_facilityTable[i].hrCode)
            {
                hr = StringCchPrintfA(pszName, cchName, "%s: 0x%lX", g_facilityTable[i].szName, hrCode);
                break;
            }
        }
        // If we still did not find it, just display the error code in hex
        if (i == countof(g_facilityTable))
        {
            hr = StringCchPrintfA(pszName, cchName, "0x%lX", hrCode);
        }
    }

    // Use a default description string for unknown errors
    if (SUCCEEDED(hr) && pszDesc)
    {
        hr = StringCchCopyA(pszDesc, cchDesc, SUCCEEDED(hrCode) ? "Unknown success code" : "Unknown error");
    }

    return hr;
}
