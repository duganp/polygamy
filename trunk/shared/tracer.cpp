/*****************************************************************************
*
* File:     tracer.cpp
* Content:  Tracing and runtime error detection.
*
*****************************************************************************/

#include "tracer.h"

#ifdef USE_TRACER

// Required system headers
#include <wtypes.h>
#include <strsafe.h>

// Include errors.cpp for ComponentErrorName and ComponentErrorDescription
#include "errors.cpp"

// Define countof() if necessary (better version at http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx)
#ifndef countof
    #define countof(a) (sizeof(a) / sizeof *(a))
#endif


/*****************************************************************************
 *
 * Identifiers and strings specific to this instance of Tracer.
 *
 *****************************************************************************/

// Create a string macro for the component name from TRACE_COMPONENT
#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)
#define TRACE_COMPONENT_NAME STRINGIFY(TRACE_COMPONENT)

// Set up a prefix for the trace messages if none has been provided.
// The prefix can be suppressed by defining TRACE_NO_MESSAGE_PREFIX.
#ifndef TRACE_NO_MESSAGE_PREFIX
    #ifndef TRACE_MESSAGE_PREFIX
        #define TRACE_MESSAGE_PREFIX TRACE_COMPONENT_NAME
    #endif
#endif


/*****************************************************************************
 *
 * Compile-time default settings for the tracing system;
 * can be overridden at runtime using the registry values above.
 *
 *****************************************************************************/

// Use default values for the default settings if unspecified.  These values
// can be overridden at runtime using the registry or a configuration file.
#ifndef TRACE_DEFAULT_LEVEL
    #define TRACE_DEFAULT_LEVEL (TraceType_Error|TraceType_Warning)
#endif
#ifndef TRACE_DEFAULT_BREAK_LEVEL
    #define TRACE_DEFAULT_BREAK_LEVEL 0
#endif
#ifndef TRACE_LOGTHREADID_DEFAULT
    #define TRACE_LOGTHREADID_DEFAULT FALSE
#endif
#ifndef TRACE_LOGFILELINE_DEFAULT
    #define TRACE_LOGFILELINE_DEFAULT FALSE
#endif
#ifndef TRACE_LOGFUNCTIONNAME_DEFAULT
    #define TRACE_LOGFUNCTIONNAME_DEFAULT FALSE
#endif
#ifndef TRACE_TIMING_DEFAULT
    #define TRACE_TIMING_DEFAULT FALSE
#endif

// The configuration structure for this instance of Tracer is defined using
// the component name as a suffix; this allows each component to have its own
// settings and makes it it easier to edit them in the debugger at runtime.
#define g_config _fix(g_TraceConfigFor) ## _fix(TRACE_COMPONENT)
static TraceConfiguration g_config =
{
    TRACE_DEFAULT_LEVEL,           // TraceMask
    TRACE_DEFAULT_BREAK_LEVEL,     // BreakMask
    TRACE_LOGTHREADID_DEFAULT,     // LogThreadID
    TRACE_LOGFILELINE_DEFAULT,     // LogFileline
    TRACE_LOGFUNCTIONNAME_DEFAULT, // LogFunctionName
    TRACE_TIMING_DEFAULT           // LogTiming
};

// Structure describing Tracer's current state.
struct TraceState
{
    BOOL Initialized;            // Whether this Tracer instance is initialized
    HANDLE LocalLogFileHandle;   // Local handle to the optional log file
};
#define g_state _fix(g_TraceStateFor) ## _fix(TRACE_COMPONENT)
static TraceState g_state = {FALSE, INVALID_HANDLE_VALUE};

// Largest size allowed for all strings used here
#define MAX_STRING_SIZE 512

// Some string constants used by ComponentTrace() below
#ifndef TRACE_CRLF
    #define TRACE_CRLF  "\r\n"
#endif
#define MSG_HOLYCOW   "################################################################################" TRACE_CRLF
#define MSG_ASSERT    TRACE_CRLF "### ASSERT FAILED: "
#define MSG_ERROR     "ERROR: "
#define MSG_WARNING   "WARNING: "
#define MSG_APICALL   "API call: "
#ifdef TRACE_NO_MESSAGE_PREFIX
    #define MSG_NAME  ""
#else
    #define MSG_NAME  TRACE_MESSAGE_PREFIX ": "
#endif


/*****************************************************************************
 *
 * Registry key and value names.
 *
 *****************************************************************************/

#define REG_KEY_NAME         "Software\\" TRACE_COMPONENT_NAME
#define REG_TRACE_MASK       "TraceMask"
#define REG_BREAK_MASK       "BreakMask"
#define REG_LOG_THREAD_IDS   "LogThreadIDs"
#define REG_LOG_TIMING       "LogTiming"
#define REG_LOG_FILELINE     "LogFilelines"
#define REG_LOG_FUNCNAMES    "LogFunctionNames"
#define REG_LOGFILE_LOCATION "LogFileLocation"


/*****************************************************************************
 *
 * AdjustTraceSettings: Enforce a valid tracing configuration.
 *
 *****************************************************************************/

static void AdjustTraceSettings(TraceConfiguration* pConfig)
{
    // Only allow breaking into the debugger on warnings or errors
    pConfig->BreakMask &= (TraceType_Warning | TraceType_Error);

    // Some trace message types imply others
    if (pConfig->TraceMask & TraceType_Warning) pConfig->TraceMask |= TraceType_Error;
    if (pConfig->TraceMask & TraceType_Detail)  pConfig->TraceMask |= TraceType_Info;
    if (pConfig->TraceMask & TraceType_FnCall)  pConfig->TraceMask |= TraceType_ApiCall;
    if (pConfig->BreakMask & TraceType_Warning) pConfig->BreakMask |= TraceType_Error;
}


/*****************************************************************************
 *
 * DisplayTraceSettings: Display the tracing configuration.
 *
 *****************************************************************************/

// Use a component-specific name for this function, so the TRACE macros will
// use the right g_config settings
#define DisplayTraceSettings _fix(Display) ## _fix(TRACE_COMPONENT) ## _fix(TraceSettings)

static void DisplayTraceSettings(const TraceConfiguration* pConfig)
{
    TRACE(DETAIL, "Changing trace output settings:");

    TRACE(DETAIL, "    Message types enabled:%s%s%s%s%s%s%s%s%s%s",
          (pConfig->TraceMask == 0)                ? " NONE"     : "",
          (pConfig->TraceMask & TraceType_Error)   ? " Errors"   : "",
          (pConfig->TraceMask & TraceType_Warning) ? " Warnings" : "",
          (pConfig->TraceMask & TraceType_Info)    ? " Info"     : "",
          (pConfig->TraceMask & TraceType_Detail)  ? " Detail"   : "",
          (pConfig->TraceMask & TraceType_ApiCall) ? " ApiCalls" : "",
          (pConfig->TraceMask & TraceType_FnCall)  ? " FnCalls"  : "",
          (pConfig->TraceMask & TraceType_Timing)  ? " Timing"   : "",
          (pConfig->TraceMask & TraceType_Locks)   ? " Locks"    : "",
          (pConfig->TraceMask & TraceType_Memory)  ? " Memory"   : "");

    UINT32 uCustomTraceTypes = pConfig->TraceMask & ~TraceType_All;
    if (uCustomTraceTypes)
    {
        TRACE(DETAIL, "    Custom message types enabled: 0x%lx", uCustomTraceTypes);
    }

    if (pConfig->LogFileline || pConfig->LogFunctionName || pConfig->LogTiming || pConfig->LogThreadID)
    {
        TRACE(DETAIL, "    Messages will include%s%s%s%s",
              pConfig->LogFileline     ? ": source files/lines" : "",
              pConfig->LogFunctionName ? ": function names"     : "",
              pConfig->LogTiming       ? ": timestamps"         : "",
              pConfig->LogThreadID     ? ": thread IDs"         : "");
    }

    if (pConfig->BreakMask)
    {
        TRACE(DETAIL, "    Will break into the debugger on:%s%s",
              (pConfig->BreakMask & TraceType_Error)   ? " Errors"   : "",
              (pConfig->BreakMask & TraceType_Warning) ? " Warnings" : "");
    }
}


/*****************************************************************************
 *
 * GetTraceSettings: Reads traceing settings from the registry.
 *
 *****************************************************************************/

static void GetTraceSettings(TraceConfiguration* pConfig)
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY_NAME, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
    {
        TRACE(DETAIL, "Reading custom tracing settings from registry");

        UINT32 uData = 0;
        PBYTE pbData = PBYTE(&uData);

        DWORD dwDataSize = sizeof uData;
        if (!RegQueryValueExA(hKey, REG_TRACE_MASK, NULL, NULL, pbData, &dwDataSize) && dwDataSize == sizeof uData)
        {
            pConfig->TraceMask = uData;
        }
        dwDataSize = sizeof uData;
        if (!RegQueryValueExA(hKey, REG_BREAK_MASK, NULL, NULL, pbData, &dwDataSize) && dwDataSize == sizeof uData)
        {
            pConfig->BreakMask = uData;
        }
        dwDataSize = sizeof uData;
        if (!RegQueryValueExA(hKey, REG_LOG_THREAD_IDS, NULL, NULL, pbData, &dwDataSize) && dwDataSize == sizeof uData)
        {
            pConfig->LogThreadID = BOOL(uData);
        }
        dwDataSize = sizeof uData;
        if (!RegQueryValueExA(hKey, REG_LOG_TIMING, NULL, NULL, pbData, &dwDataSize) && dwDataSize == sizeof uData)
        {
            pConfig->LogTiming = BOOL(uData);
        }
        dwDataSize = sizeof uData;
        if (!RegQueryValueExA(hKey, REG_LOG_FILELINE, NULL, NULL, pbData, &dwDataSize) && dwDataSize == sizeof uData)
        {
            pConfig->LogFileline = BOOL(uData);
        }
        dwDataSize = sizeof uData;
        if (!RegQueryValueExA(hKey, REG_LOG_FUNCNAMES, NULL, NULL, pbData, &dwDataSize) && dwDataSize == sizeof uData)
        {
            pConfig->LogFunctionName = BOOL(uData);
        }

        // Ensure that the settings make sense
        AdjustTraceSettings(pConfig);

        // Generate a pathname for the log file if necessary
        if (g_hTraceLogFile != INVALID_HANDLE_VALUE)
        {
            // Log file already exists (created by some other Tracer instance
            // that was initialized before us); duplicate the existing handle.
            if (!DuplicateHandle(GetCurrentProcess(), g_hTraceLogFile,
                                 GetCurrentProcess(), &g_state.LocalLogFileHandle,
                                 0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                TRACE(ERROR, "Failed to duplicate log file handle (error %lu)", GetLastError());
            }
        }
        else
        {
            char szLogFileLocation[MAX_PATH];
            dwDataSize = sizeof szLogFileLocation;
            if (!RegQueryValueExA(hKey, REG_LOGFILE_LOCATION, NULL, NULL, PBYTE(szLogFileLocation), &dwDataSize) &&
                dwDataSize > 0 && dwDataSize < MAX_PATH)
            {
                char szModuleName[MAX_PATH];
                if (GetModuleFileNameA(GetModuleHandle(NULL), szModuleName, countof(szModuleName)))
                {
                    char szPathName[MAX_PATH];
                    char* pszFileName = NULL;
                    if (GetFullPathNameA(szModuleName, countof(szModuleName), szPathName, &pszFileName))
                    {
                        char* pLastDot = NULL;
                        for (char* p = pszFileName; *p; ++p) if (*p == '.') pLastDot = p;
                        if (pLastDot) *pLastDot = '\0';

                        SYSTEMTIME systemTime;
                        GetLocalTime(&systemTime);

                        char szLogFileName[MAX_STRING_SIZE];
                        StringCchPrintfA(szLogFileName, countof(szLogFileName), "%s\\%s-%04u%02u%02u-%02u%02u%02u-" TRACE_COMPONENT_NAME ".log", szLogFileLocation,
                                         pszFileName, systemTime.wYear, systemTime.wMonth, systemTime.wDay, systemTime.wHour, systemTime.wMinute, systemTime.wSecond);

                        g_hTraceLogFile = CreateFileA(szLogFileName, FILE_WRITE_DATA, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                                                      FILE_FLAG_SEQUENTIAL_SCAN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, NULL);
                        if (g_hTraceLogFile == INVALID_HANDLE_VALUE)
                        {
                            TRACE(ERROR, "Failed to open logfile %s (error %lu)", szLogFileName, GetLastError());
                        }
                        else
                        {
                            g_state.LocalLogFileHandle = g_hTraceLogFile;
                            TRACE(DETAIL, "Enabled file logging to %s", szLogFileName);
                        }
                    }
                }
            }
        }

        RegCloseKey(hKey);
    }
}


/*****************************************************************************
 *
 * ComponentTraceBegin: Initializes the tracing system.
 *
 *****************************************************************************/

BOOL ComponentTraceBegin()
{
    if (!g_state.Initialized)
    {
        // Read user-specified tracing settings
        GetTraceSettings(&g_config);

        g_state.Initialized = TRUE;
    }

    return g_state.Initialized;
}


/*****************************************************************************
 *
 * ComponentTraceEnd: Closes down the tracing system.
 *
 *****************************************************************************/

void ComponentTraceEnd()
{
    if (g_state.LocalLogFileHandle != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers(g_state.LocalLogFileHandle);
        CloseHandle(g_state.LocalLogFileHandle);
        g_state.LocalLogFileHandle = INVALID_HANDLE_VALUE;
    }
}


/*****************************************************************************
 *
 * ComponentTraceConfigure: Changes tracing options.
 *
 *****************************************************************************/

void ComponentTraceConfigure(const TraceConfiguration* pNewConfig, BOOL bQuiet)
{
    ASSERT(pNewConfig);

    // Display the new settings now if we won't be able to after switching to them
    if (!bQuiet && (g_config.TraceMask & TraceType_Detail)
                && !(pNewConfig->TraceMask & TraceType_Detail))
    {
        DisplayTraceSettings(pNewConfig);
    }

    // Apply and validate the new settings
    CopyMemory(&g_config, pNewConfig, sizeof g_config);
    AdjustTraceSettings(&g_config);

    // Try to display them again
    if (!bQuiet)
    {
        DisplayTraceSettings(&g_config);
    }
}


/*****************************************************************************
 *
 * ComponentTracesEnabled: Returns the bitmap of enabled message types.
 *
 *****************************************************************************/

UINT32 ComponentTracesEnabled()
{
    return g_config.TraceMask;
}


/*****************************************************************************
 *
 * ComponentTrace: Writes formatted and decorated trace messages.
 * Should never be called directly, only through the TRACE macro.
 *
 * Arguments:
 *  UINT32  uType:     Type and severity of the message to be logged.
 *  PCSTR   pFile:     Source file containing the ComponentTrace call.
 *  UINT32  uLine:     Line number containing the ComponentTrace call.
 *  PCSTR   pFunction: Function containing the ComponentTrace call.
 *  PCSTR   pFormat:   Format of the log message, in printf syntax.
 *  va_list ...:       Variable argument list used to format the message.
 *
 *****************************************************************************/

void ComponentTrace
(
    UINT32 uType,
    __in_opt PCSTR pFile,
    UINT32 uLine,
    __in PCSTR pFunction,
    __in __format_string PCSTR pFormat,
    ...
)
{
    // Return immediately if this message is disabled (and isn't obligatory)
    if (!(uType & (g_config.TraceMask|g_config.BreakMask|TraceType_Always))) return;

    // Allocate message buffer on stack
    char szMsg[MAX_STRING_SIZE];
    char* pWriteCursor = szMsg;
    size_t nCharsRemaining = countof(szMsg);

    // Special handling for asserts
    if (uType == TraceType_Assert)
    {
        StringCchCopyExA(pWriteCursor, nCharsRemaining, MSG_HOLYCOW "### ", &pWriteCursor, &nCharsRemaining, 0);
    }

    // Add component name prefix
    StringCchCopyExA(pWriteCursor, nCharsRemaining, MSG_NAME, &pWriteCursor, &nCharsRemaining, 0);

    // Add information about process ID, thread ID, file, line, and function
    if (g_config.LogThreadID)
    {
        StringCchPrintfExA(pWriteCursor, nCharsRemaining, &pWriteCursor, &nCharsRemaining, 0, "Thread %x: ", GetCurrentThreadId());
    }
    if (g_config.LogTiming)
    {
        StringCchPrintfExA(pWriteCursor, nCharsRemaining, &pWriteCursor, &nCharsRemaining, 0, "%.3fms: ", double(GetPerfCounter() - g_TraceBaseTime) / g_TicksPerMs);
    }
    if ((g_config.LogFileline || uType == TraceType_Assert) && pFile)
    {
        PCSTR pLastSlash = pFile;
        for (PCSTR p=pFile; *p; ++p) // Secure because 'file' is always a null-terminated __FILE__ constant
            if (*p == '\\')
                pLastSlash = p+1;
        StringCchPrintfExA(pWriteCursor, nCharsRemaining, &pWriteCursor, &nCharsRemaining, 0, "%s:%d: ", pLastSlash, uLine);
    }
    if ((g_config.LogFunctionName || (uType & (TraceType_ApiCall|TraceType_FnCall))) && pFunction)
    {
        StringCchPrintfExA(pWriteCursor, nCharsRemaining, &pWriteCursor, &nCharsRemaining, 0, "%s: ", pFunction);
    }

    // Add a prefix according to the uType and severity of the message
    PCSTR pPrefix = (uType == TraceType_Assert) ? MSG_ASSERT  :
                    (uType & TraceType_Error)   ? MSG_ERROR   :
                    (uType & TraceType_Warning) ? MSG_WARNING :
                    (uType & TraceType_ApiCall) ? MSG_APICALL : "";
    StringCchCopyExA(pWriteCursor, nCharsRemaining, pPrefix, &pWriteCursor, &nCharsRemaining, 0);

    // Format the message
    va_list vargs;
    va_start(vargs, pFormat);
    StringCchVPrintfExA(pWriteCursor, nCharsRemaining, &pWriteCursor, &nCharsRemaining, 0, pFormat, vargs);

    // Add postfix
    StringCchCopyExA(pWriteCursor, nCharsRemaining, uType==TraceType_Assert ? TRACE_CRLF MSG_HOLYCOW : TRACE_CRLF, &pWriteCursor, &nCharsRemaining, 0);

    // Finally write the message to the debugger and possibly the log file
    OutputDebugStringA(szMsg);
    if (g_state.LocalLogFileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD nCharsToWrite = DWORD(countof(szMsg) - nCharsRemaining);
        if (!WriteFile(g_state.LocalLogFileHandle, szMsg, nCharsToWrite, &nCharsToWrite, NULL))
        {
            FlushFileBuffers(g_state.LocalLogFileHandle);
            CloseHandle(g_state.LocalLogFileHandle);
            g_state.LocalLogFileHandle = INVALID_HANDLE_VALUE;
            TRACE(ERROR, "Error %lu writing to log file; closing it", GetLastError());
        }
    }

    #if TRACE_PRINT_TO_STDOUT
        printf("%s", szMsg);
        // Using a format string here instead of printing szMsg directly keeps
        // printf() from misinterpreting any '%' characters in szMsg, so that
        // messages like "A % B == 0" won't be corrupted.
    #endif

    // Break into the debugger if appropriate for this message level
    if (uType & g_config.BreakMask)
    {
        BREAK();
    }
}
#pragma optimize("", on)


/*****************************************************************************
 *
 * ComponentScopeTrace: Utility class for logging function entries and exits.
 *
 *****************************************************************************/

ComponentScopeTrace::ComponentScopeTrace(UINT32 msgType, PCSTR pFile, UINT32 uLine, PCSTR pFunction, const void* pThis, const HRESULT* pHr)
  : m_msgType(msgType), m_file(pFile), m_line(uLine), m_function(pFunction), m_this(pThis), m_pHr(pHr)
{
    if (m_msgType & (g_config.TraceMask|g_config.BreakMask|TraceType_Always))
    {
        if (pThis)
        {
            ComponentTrace(m_msgType, pFile, uLine, pFunction, "Entry (this=0x%p)", pThis);
        }
        else
        {
            ComponentTrace(m_msgType, pFile, uLine, pFunction, "Entry");
        }
    }
}

ComponentScopeTrace::~ComponentScopeTrace()
{
    if (m_msgType & (g_config.TraceMask|g_config.BreakMask|TraceType_Always))
    {
        if (m_pHr)
        {
            if (SUCCEEDED(*m_pHr))
            {
                ComponentTrace(m_msgType, m_file, m_line, m_function, "Exit (%s)", ComponentErrorName(*m_pHr));
            }
            else
            {
                ComponentTrace(m_msgType, m_file, m_line, m_function, "FAILED (%s: %s)", ComponentErrorName(*m_pHr), ComponentErrorDescription(*m_pHr));
            }
        }
        else
        {
            ComponentTrace(m_msgType, m_file, m_line, m_function, "Exit");
        }
    }
}


#endif // USE_TRACER