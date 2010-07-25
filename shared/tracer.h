/***************************************************************************
*
* File:     tracer.h
* Content:  Tracing and runtime error detection.
*
***************************************************************************/

#ifndef TRACER_H
#define TRACER_H

// We redefine these common macros below, so undefine them first
#ifdef TRACE
    #pragma message(__FILE__ ": TRACE macro was already defined; redefining")
    #undef TRACE
#endif
#ifdef ASSERT
    #pragma message(__FILE__ ": ASSERT macro was already defined; redefining")
    #undef ASSERT
#endif
#ifdef ERROR
    #pragma message(__FILE__ ": ERROR macro was already defined; redefining")
    #undef ERROR
#endif
#ifdef NODEFAULT
    #pragma message(__FILE__ ": NODEFAULT macro was already defined; redefining")
    #undef NODEFAULT
#endif
#ifdef BREAK
    #pragma message(__FILE__ ": BREAK macro was already defined; redefining")
    #undef BREAK
#endif
#ifdef BREAK_MSG
    #pragma message(__FILE__ ": BREAK_MSG macro was already defined; redefining")
    #undef BREAK_MSG
#endif
#ifdef VERIFY
    #pragma message(__FILE__ ": VERIFY macro was already defined; redefining")
    #undef VERIFY
#endif


// QueryPerformance function wrappers
inline UINT64 GetPerfCounter()   {UINT64 n; QueryPerformanceCounter  (PLARGE_INTEGER(&n)); return n;}
inline UINT64 GetPerfFrequency() {UINT64 n; QueryPerformanceFrequency(PLARGE_INTEGER(&n)); return n;}

// DELAY_CHECKPOINT: Used to set a start point for elapsed time measurements
#define DELAY_CHECKPOINT() UINT64 _qpcCheckPoint = GetPerfCounter(), \
                                  _qpcStartPoint = _qpcCheckPoint, \
                                  _qpcOldPoint = _qpcStartPoint
// (Slight trickery: _qpcOldPoint need not be initialized here, but we set it
// to _qpcStartPoint just to make sure _qpcStartPoint is referenced somewhere
// and avoid compiler warning C4189 if TOTAL_DELAY_MEASURED() is never used)

// DELAY_MEASURED: Returns milliseconds elapsed since the last call to any of the DELAY_* macros
extern const __declspec(selectany) UINT64 g_TicksPerMs = GetPerfFrequency() / 1000;
#define DELAY_MEASURED() ((_qpcOldPoint = _qpcCheckPoint), \
                          (_qpcCheckPoint = GetPerfCounter()), \
                          float(_qpcCheckPoint - _qpcOldPoint) / g_TicksPerMs)

// TOTAL_DELAY_MEASURED: Returns milliseconds elapsed since the call to DELAY_CHECKPOINT
#define TOTAL_DELAY_MEASURED() ((_qpcOldPoint = _qpcCheckPoint), \
                                (_qpcCheckPoint = GetPerfCounter()), \
                                float(_qpcCheckPoint - _qpcStartPoint) / g_TicksPerMs)


#ifdef USE_TRACER

    #include <wtypes.h> // For HANDLE, etc.

    // Use a default component name prefix if none was specified.  This prefix
    // is also used by errors.h, so we define it before #including errors.h.
    #ifndef TRACE_COMPONENT
        #pragma message("Warning: TRACE_COMPONENT not defined; using default names.")
        #define TRACE_COMPONENT Debug
    #endif

    // Helper functions for displaying human-readable error messages
    #include "errors.h"
    #undef ERROR  // Some headers #included by errors.h can define ERROR

    // Structure used by ComponentTraceConfigure below
    struct TraceConfiguration
    {
        UINT32 TraceMask;            // Bitmap of enabled message types
        UINT32 BreakMask;            // Message types that will cause an immediate break
        BOOL LogThreadID;            // Whether to log the thread ID with each message
        BOOL LogFileline;            // Whether to log the source file and line number
        BOOL LogFunctionName;        // Whether to log the function name
        BOOL LogTiming;              // Whether to log message timestamps
    };

    // Redefine our function names based on the component name prefix; allows
    // multiple libraries to use tracer.cpp without confusing each other
    #define _fix(x) x
    #define ComponentTraceBegin     _fix(TRACE_COMPONENT) ## TraceBegin
    #define ComponentTraceEnd       _fix(TRACE_COMPONENT) ## TraceEnd
    #define ComponentTraceConfigure _fix(TRACE_COMPONENT) ## TraceConfigure
    #define ComponentTracesEnabled  _fix(TRACE_COMPONENT) ## TracesEnabled
    #define ComponentTrace          _fix(TRACE_COMPONENT) ## Trace
    #define ComponentScopeTrace     _fix(TRACE_COMPONENT) ## ScopeTrace

    // Initialization, termination, configuration
    BOOL ComponentTraceBegin();
    void ComponentTraceEnd();
    void ComponentTraceConfigure(const TraceConfiguration*, BOOL bQuiet);
    UINT32 ComponentTracesEnabled();

    // ComponentTrace: Writes formatted and decorated messages to the debugger
    void ComponentTrace(UINT32 uType, __in_opt PCSTR pFile, UINT32 uLine, __in PCSTR pFunction, __in __format_string PCSTR pFormat, ...);

    // Message type/severity bits for ComponentTrace
    static const UINT32
        TraceType_Error      = 0x00000001,  // For handled errors with serious effects
        TraceType_Warning    = 0x00000002,  // For handled errors that may be recoverable
        TraceType_Info       = 0x00000004,  // Informational chit-chat (e.g. state changes)
        TraceType_Detail     = 0x00000008,  // More detailed chit-chat
        TraceType_ApiCall    = 0x00000010,  // Public API function entries and exits
        TraceType_FnCall     = 0x00000020,  // Internal function entries and exits
        TraceType_Timing     = 0x00000040,  // Delays detected and other timing data
        TraceType_Locks      = 0x00000080,  // Usage of critical sections and mutexes
        TraceType_Memory     = 0x00000100,  // Memory heap usage information
        TraceType_All        = 0x000001ff,  // Represents all standard message types
        TraceType_Always     = 0x80000000,  // For messages that must be displayed
        TraceType_Assert     = 0xC0000000;  // Only used by the ASSERT macro family

    // Macros to make calling ComponentTrace() easier
    #define TRACE      ComponentTrace
    #define ERROR      TraceType_Error,     __FILE__, __LINE__, __FUNCTION__
    #define WARNING    TraceType_Warning,   __FILE__, __LINE__, __FUNCTION__
    #define INFO       TraceType_Info,      __FILE__, __LINE__, __FUNCTION__
    #define DETAIL     TraceType_Detail,    __FILE__, __LINE__, __FUNCTION__
    #define APICALLS   TraceType_ApiCall,   __FILE__, __LINE__, __FUNCTION__
    #define FNCALLS    TraceType_FnCall,    __FILE__, __LINE__, __FUNCTION__
    #define TIMING     TraceType_Timing,    __FILE__, __LINE__, __FUNCTION__
    #define LOCKS      TraceType_Locks,     __FILE__, __LINE__, __FUNCTION__
    #define MEMORY     TraceType_Memory,    __FILE__, __LINE__, __FUNCTION__
    #define ALWAYS     TraceType_Always,    __FILE__, __LINE__, __FUNCTION__

    // Utility class for logging function entries and exits
    struct ComponentScopeTrace
    {
        UINT32 m_msgType;
        PCSTR m_file;
        UINT32 m_line;
        PCSTR m_function;
        const void* m_this;
        const HRESULT* m_pHr;
        ComponentScopeTrace(UINT32 msgType, PCSTR pFile, UINT32 uLine, PCSTR pFunction, const void* pThis, const HRESULT* pHr);
        ~ComponentScopeTrace();
    };

    // Macros for automated logging of function/method calls and return codes
    #define TRACE_API(hr, api)      ComponentScopeTrace _ScopeTrace(TraceType_ApiCall, __FILE__, __LINE__, (api), this, &(hr))
    #define TRACE_API_FUNCTION(hr)  ComponentScopeTrace _ScopeTrace(TraceType_ApiCall, __FILE__, __LINE__, __FUNCTION__, NULL, &(hr))
    #define TRACE_VOID_API(api)     ComponentScopeTrace _ScopeTrace(TraceType_FnCall,  __FILE__, __LINE__, (api), this, NULL)
    #define TRACE_METHOD(hr)        ComponentScopeTrace _ScopeTrace(TraceType_FnCall,  __FILE__, __LINE__, __FUNCTION__, this, &(hr))
    #define TRACE_VOID_METHOD()     ComponentScopeTrace _ScopeTrace(TraceType_FnCall,  __FILE__, __LINE__, __FUNCTION__, this, NULL)
    #define TRACE_FUNCTION(hr)      ComponentScopeTrace _ScopeTrace(TraceType_FnCall,  __FILE__, __LINE__, __FUNCTION__, NULL, &(hr))
    #define TRACE_VOID_FUNCTION()   ComponentScopeTrace _ScopeTrace(TraceType_FnCall,  __FILE__, __LINE__, __FUNCTION__, NULL, NULL)

    // TRACE_FAILURE: Used to log unexpected function call failures that are
    // checked for and possibly handled in the retail build.
    #define TRACE_FAILURE(hr, s) {if (FAILED(hr)) TRACE(ERROR, s " failed (%s: %s)", \
                                  ComponentErrorName(hr), ComponentErrorDescription(hr));}

    // NODEFAULT: A macro to use in default switch cases that should never be hit
    #define NODEFAULT BREAK_MSG("Should never reach the default branch of this switch")

    // Simple BREAK and ASSERT macros.  They have the useful property of breaking
    // at the point of failure, not in a nested function call like assert().
    #ifdef _X86_
        #define BREAK() {__asm {int 3}; __assume(0);}
    #else
        #define BREAK() {__debugbreak(); __assume(0);}
    #endif
    #define BREAK_MSG(s) {ComponentTrace(TraceType_Assert, __FILE__, __LINE__, __FUNCTION__, "%s", (s)); BREAK();}
    #define ASSERT(x) {if (!(x)) BREAK_MSG(#x); __assume(x);}

    // Verification macros: Check an operation's result.  If it failed, display
    // information about the error and break into the debugger.  These macros
    // should be used for calls that should only ever fail on a broken system.
    // For performance reasons the checks only happen in the debug build.

    // VERIFY: Used for operations that return an HRESULT
    #define VERIFY(x) {HRESULT _hr = (x); if (FAILED(_hr)) {TRACE(ALWAYS, #x " failed (%s: %s)", \
                       ComponentErrorName(_hr), ComponentErrorDescription(_hr)); BREAK();}}

    // VERIFY_WIN32: Used for Win32 API calls that report failure by returning 0
    // and setting the thread's last-error code.
    #define VERIFY_WIN32(x) {if (!(x)) {HRESULT _hr = HRESULT_FROM_WIN32(GetLastError()); \
                             TRACE(ALWAYS, #x " failed (%s: %s)", ComponentErrorName(_hr), \
                             ComponentErrorDescription(_hr)); BREAK();}}

    // VERIFY_ZERO: Like VERIFY for calls that indicate success by returning 0
    #define VERIFY_ZERO(x) {DWORD _rc = DWORD(x); if (_rc) {TRACE(ALWAYS, #x " == %d (should be 0)", _rc); BREAK();}}

    // VERIFY_PTR: For calls that indicate success by returning non-NULL
    #define VERIFY_PTR(x) {if (!(x)) {TRACE(ALWAYS, #x " is NULL"); BREAK();}}

    // VERIFY_TRUE: For calls that indicate success by returning TRUE
    #define VERIFY_TRUE(x) {if (!(x)) {TRACE(ALWAYS, #x " is not TRUE"); BREAK();}}

    // Handle to an optional log file shared across all instances of Tracer
    extern __declspec(selectany) HANDLE g_hTraceLogFile = INVALID_HANDLE_VALUE;

    // Store the initial time so we can make message timestamps 0-based
    extern __declspec(selectany) UINT64 g_TraceBaseTime = GetPerfCounter();

#else // Definitions for retail builds

    #pragma warning(disable:4002) // Avoid "too many actual parameters for macro" warning
    #define ComponentTraceBegin()
    #define ComponentTraceEnd()
    #define ComponentTraceConfigure()
    #define TRACE()
    #define TRACE_API(hr, api)
    #define TRACE_API_FUNCTION(hr)
    #define TRACE_VOID_API(api)
    #define TRACE_METHOD(hr)
    #define TRACE_VOID_METHOD()
    #define TRACE_FUNCTION(hr)
    #define TRACE_VOID_FUNCTION()
    #define TRACE_FAILURE(hr, s)    (hr)  // Avoids error C4189 (unreferenced local variable)
    #define NODEFAULT               __assume(0)
    #define BREAK()                 __assume(0)
    #define BREAK_MSG(s)            __assume(0)
    #define ASSERT(x)               __assume(x)
    #define VERIFY(x)               {(x); __assume(SUCCEEDED(x));}
    #define VERIFY_WIN32(x)         {(x); __assume((x) != 0);}
    #define VERIFY_ZERO(x)          {(x); __assume((x) == 0);}
    #define VERIFY_PTR(x)           {(x); __assume((x) != NULL);}
    #define VERIFY_TRUE(x)          {(x); __assume(x);}

#endif // USE_TRACER

#endif // TRACER_H
