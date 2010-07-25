/***************************************************************************
*
* File:     threader.h
* Content:  Template classes for threading support.
*
***************************************************************************/

#ifndef THREADER_H
#define THREADER_H

#include <strsafe.h>    // For StringCbCopyA and SAL annotations

#include "tracer.h"     // For TRACE, ASSERT, BREAK_MSG, etc.
#include "grab-bag.h"   // For CLOSE_HANDLE, AtomicRead, etc.


// Warn if acquiring a lock takes longer than LOCK_ACQUIRE_DELAY milliseconds
#define LOCK_ACQUIRE_DELAY 1.0

// Warn if a timed wait returns more than SLEEP_DURATION_ERROR ms early or late
#define SLEEP_DURATION_ERROR 2.0

// An inline function to call CloseHandle as safely as possible
static inline void SAFE_CLOSE_HANDLE(HANDLE& handle)
{
    if (VALID_HANDLE(handle))
    {
        if (!CloseHandle(handle))
        {
            TRACE(ERROR, "Failed to close handle 0x%lX: error=%ld", handle, GetLastError());
            BREAK();
        }
        handle = 0;
    }
}



///////////////////////////////////////////////////////////////////////////////
//
// Definitions needed to emulate GetThreadId() for Windows versions in which
// it is not available (XP and prior).
//
// FIXME: May be able to just #include <winternl.h> instead.
//
///////////////////////////////////////////////////////////////////////////////

// Function pointer type for NtQueryInformationThread
extern "C" typedef LONG (__stdcall NtQueryInformationThreadFuncType)
(
    __in HANDLE ThreadHandle,
    __in int /*THREADINFOCLASS*/ ThreadInformationClass,
    __out_bcount(ThreadInformationLength) PVOID ThreadInformation,
    __in ULONG ThreadInformationLength,
    __out_opt PULONG ReturnLength
);

// Data returned by NtQueryInformationThread
typedef struct _CLIENT_ID
{
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID;

typedef struct _THREAD_BASIC_INFORMATION
{
    LONG /*NTSTATUS*/ ExitStatus;
    void* /*PTEB*/ TebBaseAddress;
    CLIENT_ID ClientId;
    ULONG_PTR AffinityMask;
    LONG /*KPRIORITY*/ Priority;
    LONG BasePriority;
} THREAD_BASIC_INFORMATION;



///////////////////////////////////////////////////////////////////////////////
//
// ThreadBase: A helper class for thread management.  To use it, derive your
// class T privately from ThreadBase, call ThreadBase::Initialize from T's
// creation code, and implement a T::ThreadProc method, using ThreadSleep
// to yield the CPU as appropriate and terminating when it returns FALSE.
//
// Methods for thread state control: StartThread, StopThread, PauseThread,
// WakeThread, and SetWakeEvent.  These methods are themselves thread-safe.
//
// Methods to query the thread state: IsThreadRunning, IsThreadStopping and
// GetThreadIdentifier.
//
///////////////////////////////////////////////////////////////////////////////

class ThreadBase
{
    BOOL m_bInitialized;
    HANDLE m_hThread;
    DWORD m_nThreadId;
    HANDLE m_hControlEvents[3];
    HANDLE& StopEvent() {return m_hControlEvents[0];}
    HANDLE& PauseEvent() {return m_hControlEvents[1];}
    HANDLE& WakeEvent() {return m_hControlEvents[2];}
    enum {THREAD_STOP = WAIT_OBJECT_0, THREAD_PAUSE, THREAD_WAKE};
    CRITICAL_SECTION m_threadLock;
    static DWORD WINAPI StaticThreadProc(void*);
    static NtQueryInformationThreadFuncType* m_pNtQueryInformationThread;

protected:

    ThreadBase() : m_bInitialized(FALSE), m_hThread(0), m_nThreadId(0)
    {
        WakeEvent() = PauseEvent() = StopEvent() = 0;

        // Obtain a pointer to the internal NtQueryInformationThread API
        if (m_pNtQueryInformationThread == NULL)
        {
            HMODULE hNtDll = LoadLibraryA("ntdll.dll");
            ASSERT(VALID_HANDLE(hNtDll));
            m_pNtQueryInformationThread = (NtQueryInformationThreadFuncType*)(void*)
                                          GetProcAddress(hNtDll, "NtQueryInformationThread");
            ASSERT(m_pNtQueryInformationThread);
        }
    }
    virtual void ThreadProc() =0;

    DWORD GetThreadIdentifier() const {return AtomicRead(m_nThreadId);}
    BOOL IsThreadRunning() const {return MAKEBOOL(GetThreadIdentifier());}
    BOOL IsThreadStopping() {return WaitForSingleObject(StopEvent(), 0) == WAIT_OBJECT_0;}

    // Initialize must be called immediately after creating ThreadBase.  It
    // can be called multiple times if it fails, but only once if it succeeds.
    // No other calls should be made until Initialize is called successfully.

    HRESULT Initialize()
    {
        ASSERT(!m_bInitialized);
        __try {InitializeCriticalSection(&m_threadLock);}
        __except (GetExceptionCode() == STATUS_NO_MEMORY ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {return E_OUTOFMEMORY;}

        // Create the thread control events
        WakeEvent() = CreateEvent(NULL, FALSE, FALSE, NULL); // Auto-reset
        HRESULT hr = HRFROMHANDLE(WakeEvent());
        if (SUCCEEDED(hr))
        {
            PauseEvent() = CreateEvent(NULL, FALSE, FALSE, NULL); // Auto-reset
            hr = HRFROMHANDLE(PauseEvent());
        }
        if (SUCCEEDED(hr))
        {
            StopEvent() = CreateEvent(NULL, TRUE, FALSE, NULL); // Manual-reset
            hr = HRFROMHANDLE(StopEvent());
        }
        if (SUCCEEDED(hr))
        {
            m_bInitialized = TRUE;
        }
        else
        {
            if (VALID_HANDLE(PauseEvent()))
            {
                VERIFY_TRUE(CloseHandle(PauseEvent()));
                PauseEvent() = 0;
            }
            if (VALID_HANDLE(WakeEvent()))
            {
                VERIFY_TRUE(CloseHandle(WakeEvent()));
                WakeEvent() = 0;
            }
            DeleteCriticalSection(&m_threadLock);
        }

        return hr;
    }

    virtual ~ThreadBase()
    {
        if (m_bInitialized)
        {
            StopThread();

            ASSERT(VALID_HANDLE(WakeEvent()));
            VERIFY_TRUE(CloseHandle(WakeEvent()));

            ASSERT(VALID_HANDLE(PauseEvent()));
            VERIFY_TRUE(CloseHandle(PauseEvent()));

            ASSERT(VALID_HANDLE(StopEvent()));
            VERIFY_TRUE(CloseHandle(StopEvent()));

            DeleteCriticalSection(&m_threadLock);
        }
    }

    // Creates the thread in the given initial state with the given preferred
    // CPU mask, scheduling resolution, and stack size.  If the initial state
    // is Paused, the client must call WakeThread to start thread processing.

    enum InitialThreadState {Running, Paused};
    HRESULT StartThread(InitialThreadState initialState =Running,
                        int nPriority =THREAD_PRIORITY_NORMAL,
                        UINT32 uProcessorCode =0, size_t nStackSize =0)
    {
        ASSERT(m_bInitialized);
        EnterCriticalSection(&m_threadLock);
        TRACE_VOID_METHOD();

        // Ensure that the thread was not already created
        ASSERT(!m_hThread);
        ASSERT(!m_nThreadId);

        // Reset the thread's stopped state
        ASSERT(VALID_HANDLE(StopEvent()));
        VERIFY_TRUE(ResetEvent(StopEvent()));

        m_hThread = CreateThread(NULL, nStackSize, StaticThreadProc, this, CREATE_SUSPENDED, NULL);
        HRESULT hr = HRFROMP(m_hThread);

        if (SUCCEEDED(hr) && uProcessorCode)
        {
            // We treat uProcessorCode as a thread affinity mask; however,
            // for SetThreadAffinityMask to succeed, we first have to AND
            // uProcessorCode with the process and system affinity masks,
            // as the thread mask has to be a subset of these two.
            DWORD_PTR uProcessAffinityMask = 0;
            DWORD_PTR uSystemAffinityMask = 0;
            VERIFY_TRUE(GetProcessAffinityMask(GetCurrentProcess(), &uProcessAffinityMask, &uSystemAffinityMask));
            UINT32 uThreadAffinityMask = UINT32(uProcessorCode & uProcessAffinityMask);
            if (!SetThreadAffinityMask(m_hThread, uThreadAffinityMask))
            {
                TRACE(WARNING, "Unable to set thread affinity to 0x%lX", uProcessorCode);
            }
            TRACE(DETAIL, "Affinity masks: system = 0x%Ix, process = 0x%Ix, thread = 0x%Ix", uSystemAffinityMask, uProcessAffinityMask, uThreadAffinityMask);
        }

        if (SUCCEEDED(hr))
        {
            if (!SetThreadPriority(m_hThread, nPriority))
            {
                TRACE(WARNING, "Unable to set thread priority to %d", nPriority);
            }

            // Obtain the new thread's identifier.  Using GetThreadId() directly
            // would be preferable, but it is not available on Windows XP.
            THREAD_BASIC_INFORMATION tbi;
            VERIFY_ZERO(m_pNtQueryInformationThread(m_hThread, 0, &tbi, sizeof tbi, NULL));
            m_nThreadId = DWORD(DWORD_PTR(HandleToULong(tbi.ClientId.UniqueThread)));

            // Unblock the thread now that we have configured it as needed
            VERIFY_TRUE(ResumeThread(m_hThread) == 1);

            // Only let the thread proceed if initialState is Running
            if (initialState == Running)
            {
                WakeThread();
            }
        }
        else // Thread creation failed
        {
            DWORD error = GetLastError();
            TRACE(ERROR, "Failed to create thread: Win32 error %lu", error);
            hr = __HRESULT_FROM_WIN32(error);
        }

        LeaveCriticalSection(&m_threadLock);
        return hr;
    }

    // If the thread exists, stops it and returns once it is fully destroyed.

    void StopThread()
    {
        ASSERT(m_bInitialized);
        EnterCriticalSection(&m_threadLock);
        TRACE_VOID_METHOD();

        if (m_hThread)
        {
            ASSERT(VALID_HANDLE(StopEvent()));
            VERIFY_TRUE(SetEvent(StopEvent()));
            VERIFY_ZERO(WaitForSingleObject(m_hThread, INFINITE));
            CLOSE_HANDLE(m_hThread);
        }

        LeaveCriticalSection(&m_threadLock);
    }

    // Pauses the thread the next time it calls ThreadSleep.

    void PauseThread()
    {
        ASSERT(m_bInitialized);
        EnterCriticalSection(&m_threadLock);
        TRACE_VOID_METHOD();

        ASSERT(m_hThread);
        ASSERT(m_nThreadId);
        ASSERT(VALID_HANDLE(PauseEvent()));
        VERIFY_TRUE(SetEvent(PauseEvent()));

        LeaveCriticalSection(&m_threadLock);
    }

    // Prompts the thread to to run immediately.

    void WakeThread()
    {
        ASSERT(m_bInitialized);
        EnterCriticalSection(&m_threadLock);
        TRACE_VOID_METHOD();

        ASSERT(m_hThread);
        ASSERT(m_nThreadId);
        ASSERT(VALID_HANDLE(WakeEvent()));
        VERIFY_TRUE(SetEvent(WakeEvent()));

        LeaveCriticalSection(&m_threadLock);
    }

    // Replaces the internal event handle used to wake the thread with one
    // provided by the client.  Useful when using external components that
    // signal an event periodically.  Can only be called when the thread is
    // stopped.

    void SetWakeEvent(HANDLE hNewWakeEvent)
    {
        ASSERT(m_bInitialized);
        EnterCriticalSection(&m_threadLock);
        TRACE_VOID_METHOD();

        ASSERT(!m_hThread);
        ASSERT(!m_nThreadId);
        ASSERT(VALID_HANDLE(WakeEvent()));
        ASSERT(VALID_HANDLE(hNewWakeEvent));
        VERIFY_TRUE(CloseHandle(WakeEvent()));
        VERIFY_TRUE(DuplicateHandle(GetCurrentProcess(), hNewWakeEvent, GetCurrentProcess(),
                                      &WakeEvent(), 0, FALSE, DUPLICATE_SAME_ACCESS));

        LeaveCriticalSection(&m_threadLock);
    }

    // Used within the client-provided thread procedure to pause execution for
    // uMilliseconds or until WakeThread() is called.  The thread procedure
    // should return when this method returns FALSE.  Behavior:
    //
    // 1. If StopThread() was called or WaitForMultipleObjects() returns an
    //    unexpected error code, return FALSE.
    //
    // 2. Else if PauseThread() was called, wait forever for the wake event
    //    to be signaled via WakeThread(), then return TRUE.
    //
    // 3. Else if WakeThread() was called or the time-out expires, return TRUE.

    BOOL ThreadSleep(UINT32 uMilliseconds)
    {
        ASSERT(m_bInitialized);
        TRACE_VOID_METHOD();

        #if 0  // Detailed status logging
            DELAY_CHECKPOINT();
        #endif

        DWORD dwWaitCode = WaitForMultipleObjects(countof(m_hControlEvents), m_hControlEvents, FALSE, uMilliseconds);

        #if 0  // Detailed status logging
            TRACE(INFO, "WaitForMultipleObjects returned %s",
                  dwWaitCode == WAIT_TIMEOUT ? "WAIT_TIMEOUT" :
                  dwWaitCode == WAIT_ABANDONED ? "WAIT_ABANDONED" :
                  dwWaitCode == THREAD_STOP ? "Stopped" :
                  dwWaitCode == THREAD_PAUSE ? "Paused" :
                  dwWaitCode == THREAD_WAKE ? "Woken" : "unknown return code");

            if (dwWaitCode == WAIT_TIMEOUT)
            {
                float fError = float(uMilliseconds - DELAY_MEASURED());
                if (fError > SLEEP_DURATION_ERROR)
                {
                    TRACE(TIMING, "Slept %.2fms less than requested time %lums", fError, uMilliseconds);
                }
                else if (fError < -SLEEP_DURATION_ERROR)
                {
                    TRACE(TIMING, "Slept %.2fms longer than requested time %lums", -fError, uMilliseconds);
                }
            }
        #endif

        // If the thread was paused, wait until it is stopped or woken
        while (dwWaitCode == THREAD_PAUSE)
        {
            dwWaitCode = WaitForMultipleObjects(countof(m_hControlEvents), m_hControlEvents, FALSE, INFINITE);
        }

        return (dwWaitCode == WAIT_TIMEOUT || dwWaitCode == THREAD_WAKE);
    }
};


// Define the class-static m_pNtQueryInformationThread pointer
__declspec(selectany) NtQueryInformationThreadFuncType* ThreadBase::m_pNtQueryInformationThread = NULL;


// Static thread procedure used internally to invoke the client-provided one
inline DWORD WINAPI ThreadBase::StaticThreadProc(void* lpvParam)
{
    TRACE_VOID_FUNCTION();

    ThreadBase* pThis = static_cast<ThreadBase*>(lpvParam);
    pThis->ThreadSleep(INFINITE); // Wait until the thread is woken or stopped
    pThis->ThreadProc(); // Invoke the user's thread procedure
    AtomicAssign(&pThis->m_nThreadId, DWORD(0));

    return 0;
}



///////////////////////////////////////////////////////////////////////////////
//
// IPortableLock: Interface provided by all the lock implementations below.
//
///////////////////////////////////////////////////////////////////////////////

// Macros used to generate a lock contention trace in debug builds
#ifdef USE_TRACER
    #define MAX_LOCK_NAME_SIZE 32
    #define MAX_LOCK_FUNCTION_SIZE 128
    #define LOCK_DETAILS(name) #name, __FUNCTION__
    #define ENTER_LOCK(lock) ((lock)->Enter(__FILE__, __LINE__, __FUNCTION__))
    #define TRY_ENTER_LOCK(lock) ((lock)->TryEnter(__FILE__, __LINE__, __FUNCTION__))
    #define ENTER_LOCK_OR_EVENT(lock, event) ((lock)->EnterOrEvent((event), __FILE__, __LINE__, __FUNCTION__))
    #define LEAVE_LOCK(lock) ((lock)->Leave(__FILE__, __LINE__, __FUNCTION__))
#else
    #define LOCK_DETAILS(name)
    #define ENTER_LOCK(lock) ((lock)->Enter())
    #define TRY_ENTER_LOCK(lock) ((lock)->TryEnter())
    #define ENTER_LOCK_OR_EVENT(lock, event) ((lock)->EnterOrEvent(event))
    #define LEAVE_LOCK(lock) ((lock)->Leave())
#endif

interface __declspec(novtable) IPortableLock
{
    virtual ~IPortableLock() {}

    #ifdef USE_TRACER
        // Initializes the lock.  May fail with E_OUTOFMEMORY.
        virtual HRESULT Initialize(__in_z PCSTR pszLockName, __in_z PCSTR pszCreatorFunction) =0;

        // Acquires the lock.  May block for an infinite time.
        virtual void Enter(__in_z PCSTR pszFile, UINT32 uLine, __in_z PCSTR pszCallingFunction) =0;

        // Tries to acquire the lock.  Returns immediately on failure.
        // Returns TRUE if the lock was acquired, FALSE otherwise.
        virtual BOOL TryEnter(__in_z PCSTR pszFile, UINT32 uLine, __in_z PCSTR pszCallingFunction) =0;

        // Tries to acquire the lock but also waits for an event, such as a
        // thread termination event.  Returns TRUE if the lock was acquired,
        // FALSE if the event was signaled.
        virtual BOOL EnterOrEvent(HANDLE hEvent, __in_z PCSTR pszFile, UINT32 uLine, __in_z PCSTR pszCallingFunction) =0;

        // Releases the lock.
        virtual void Leave(__in_z PCSTR pszFile, UINT32 uLine, __in_z PCSTR pszCallingFunction) =0;
    #else
        virtual HRESULT Initialize() =0;
        virtual void Enter() =0;
        virtual BOOL TryEnter() =0;
        virtual BOOL EnterOrEvent(HANDLE hEvent) =0;
        virtual void Leave() =0;
    #endif
};



///////////////////////////////////////////////////////////////////////////////
//
// CCriticalSectionLock: A helper class for critical section management.
//
///////////////////////////////////////////////////////////////////////////////

class CCriticalSectionLock : public IPortableLock
{
    CRITICAL_SECTION m_critsec;
    BOOL m_bInitialized;
    #ifdef USE_TRACER
        // For lock contention tracing
        char m_szLockName[MAX_LOCK_NAME_SIZE];
        char m_szCreator[MAX_LOCK_FUNCTION_SIZE];
    #endif

public:

    CCriticalSectionLock() : m_bInitialized(FALSE) {}

    ~CCriticalSectionLock()
    {
        if (m_bInitialized)
        {
            // Make sure no one is holding the critical section before we delete it
            EnterCriticalSection(&m_critsec);
            LeaveCriticalSection(&m_critsec);
            DeleteCriticalSection(&m_critsec);
        }
    }

    BOOL Initialized() const {return m_bInitialized;}

    // Initialize the critical section, dealing with the infamous out-of-memory exception.
    // NOTE: This class is designed for use only on XP and beyond.  (On prior versions of
    // Windows, EnterCriticalSection() could raise an exception as well.)

    #ifdef USE_TRACER
    HRESULT Initialize(__in_z PCSTR pszLockName, __in_z PCSTR pszCreatorFunction)
    #else
    HRESULT Initialize()
    #endif
    {
        ASSERT(!m_bInitialized);

        __try {InitializeCriticalSection(&m_critsec);}
        __except (GetExceptionCode() == STATUS_NO_MEMORY ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {return E_OUTOFMEMORY;}

        m_bInitialized = TRUE;
        #ifdef USE_TRACER
            VERIFY(StringCbCopyA(m_szLockName, sizeof m_szLockName, pszLockName));
            VERIFY(StringCbCopyA(m_szCreator, sizeof m_szCreator, pszCreatorFunction));
        #endif
        return S_OK;
    }

    #ifdef USE_TRACER
    void Enter(__in_z PCSTR pszFile, UINT32 uLine, __in_z PCSTR pszCallingFunction)
    #else
    void Enter()
    #endif
    {
        ASSERT(m_bInitialized);
        DELAY_CHECKPOINT();
        TRACE(TraceType_Locks, pszFile, uLine, pszCallingFunction,
              "Acquiring %s at 0x%p created by %s...", m_szLockName, this, m_szCreator);

        EnterCriticalSection(&m_critsec);

        float fDelay = DELAY_MEASURED();
        if (fDelay > LOCK_ACQUIRE_DELAY)
        {
            TRACE(TraceType_Locks|TraceType_Timing, pszFile, uLine, pszCallingFunction,
                  "Took %.2fms to acquire %s at 0x%p created by %s",
                  fDelay, m_szLockName, this, m_szCreator);
        }
        else
        {
            TRACE(TraceType_Locks, pszFile, uLine, pszCallingFunction,
                  "Acquired %s at 0x%p created by %s", m_szLockName, this, m_szCreator);
        }
    }

    #ifdef USE_TRACER
    BOOL TryEnter(__in_z PCSTR pszFile, UINT32 uLine, __in_z PCSTR pszCallingFunction)
    #else
    BOOL TryEnter()
    #endif
    {
        ASSERT(m_bInitialized);
        BOOL bAcquired = BOOL(TryEnterCriticalSection(&m_critsec));
        TRACE(TraceType_Locks, pszFile, uLine, pszCallingFunction,
              bAcquired ? "Immediately acquired %s at 0x%p created by %s" :
                          "Failed to immediately acquire %s at 0x%p created by %s",
              m_szLockName, this, m_szCreator);
        return bAcquired;
    }

    // Note: since critical sections do not support WaitForMultipleObjects
    // semantics, we cannot implement IPortableLock::EnterOrEvent for them.

    #ifdef USE_TRACER
    BOOL EnterOrEvent(HANDLE, __in_z PCSTR, UINT32, __in_z PCSTR)
    #else
    BOOL EnterOrEvent(HANDLE)
    #endif
    {
        BREAK_MSG("EnterOrEvent not supported on CCriticalSectionLock");
    }

    #ifdef USE_TRACER
    void Leave(__in_z PCSTR pszFile, UINT32 uLine, __in_z PCSTR pszCallingFunction)
    #else
    void Leave()
    #endif
    {
        ASSERT(m_bInitialized);
        TRACE(TraceType_Locks, pszFile, uLine, pszCallingFunction,
              "Releasing %s at 0x%p created by %s", m_szLockName, this, m_szCreator);
        LeaveCriticalSection(&m_critsec);
    }

    // Methods relating to the critical section's ownership

    HANDLE OwningThread() const {return m_critsec.OwningThread;}

    BOOL IsOwnedByThisThread() const
    {
        return HandleToULong(m_critsec.OwningThread) == GetCurrentThreadId();
    }
};



#ifdef NOT_IN_USE // Beware: this code may be rusty

///////////////////////////////////////////////////////////////////////////////
//
// CMutexLock: A helper class for mutex management.
//
///////////////////////////////////////////////////////////////////////////////

class CMutexLock : public IPortableLock
{
    HANDLE m_hMutex;
    BOOL m_bInitialized;
    #ifdef USE_TRACER
        // For lock contention tracing
        char m_szLockName[MAX_LOCK_NAME_SIZE];
        char m_szCreator[MAX_LOCK_FUNCTION_SIZE];
    #endif

public:

    CMutexLock() : m_bInitialized(FALSE) {}

    ~CMutexLock()
    {
        if (m_bInitialized)
        {
            // Make sure no one is holding the lock before we delete it
            #ifdef USE_TRACER
                Enter(__FILE__, __LINE__, __FUNCTION__);
            #else
                Enter();
            #endif
            VERIFY_TRUE(CloseHandle(m_hMutex));
        }
    }

    #ifdef USE_TRACER
    HRESULT Initialize(__in_z PCSTR pszLockName, __in_z PCSTR pszCreatorFunction)
    #else
    HRESULT Initialize()
    #endif
    {
        ASSERT(!m_bInitialized);
        m_hMutex = CreateMutex(NULL, FALSE, NULL);
        if (VALID_HANDLE(m_hMutex))
        {
            m_bInitialized = TRUE;
            #ifdef USE_TRACER
                VERIFY(StringCbCopyA(m_szLockName, sizeof m_szLockName, pszLockName));
                VERIFY(StringCbCopyA(m_szCreator, sizeof m_szCreator, pszCreatorFunction));
            #endif
            return S_OK;
        }
        else
        {
            DWORD error = GetLastError();
            return __HRESULT_FROM_WIN32(error);
        }
    }

    #ifdef USE_TRACER
    void Enter(__in_z PCSTR pszFile, UINT32 uLine, __in_z PCSTR pszCallingFunction)
    #else
    void Enter()
    #endif
    {
        ASSERT(m_bInitialized);
        DELAY_CHECKPOINT();
        VERIFY_ZERO(WaitForSingleObject(m_hMutex, INFINITE));
        float fDelay = DELAY_MEASURED();
        if (fDelay > LOCK_ACQUIRE_DELAY)
        {
            TRACE(TraceType_Locks|TraceType_Timing, pszFile, uLine, pszCallingFunction,
                  "Took %.2fms to acquire %s at 0x%p created by %s",
                  fDelay, m_szLockName, this, m_szCreator);
        }
        else
        {
            TRACE(TraceType_Locks, pszFile, uLine, pszCallingFunction,
                  "Acquired %s at 0x%p created by %s", m_szLockName, this, m_szCreator);
        }
    }

    #ifdef USE_TRACER
    BOOL EnterOrEvent(HANDLE hEvent, __in_z PCSTR pszFile, UINT32 uLine, __in_z PCSTR pszCallingFunction)
    #else
    BOOL EnterOrEvent(HANDLE hEvent)
    #endif
    {
        ASSERT(m_bInitialized);
        ASSERT(VALID_HANDLE(hEvent));
        DELAY_CHECKPOINT();
        HANDLE handles[2] = {hEvent, m_hMutex};
        DWORD dwWait = WaitForMultipleObjects(countof(handles), handles, FALSE, INFINITE);
        ASSERT(dwWait == WAIT_OBJECT_0 || dwWait == WAIT_OBJECT_0 + 1);
        float fDelay = DELAY_MEASURED();
        if (fDelay > LOCK_ACQUIRE_DELAY)
        {
            TRACE(TraceType_Locks|TraceType_Timing, pszFile, uLine, pszCallingFunction,
                  "Took %.2fms to acquire %s at 0x%p created by %s",
                  fDelay, m_szLockName, this, m_szCreator);
        }
        else
        {
            TRACE(TraceType_Locks, pszFile, uLine, pszCallingFunction,
                  "Acquired %s at 0x%p created by %s", m_szLockName, this, m_szCreator);
        }
        return dwWait == WAIT_OBJECT_0 + 1;
    }

    #ifdef USE_TRACER
    void Leave(__in_z PCSTR pszFile, UINT32 uLine, __in_z PCSTR pszCallingFunction)
    #else
    void Leave()
    #endif
    {
        ASSERT(m_bInitialized);
        TRACE(TraceType_Locks, pszFile, uLine, pszCallingFunction,
              "Releasing %s at 0x%p created by %s", m_szLockName, this, m_szCreator);
        ReleaseMutex(m_hMutex);
    }
};

#endif // NOT_IN_USE


#endif // THREADER_H