/***************************************************************************
*
* File:     grab-bag.h
* Content:  Various utility macros, inline functions and function templates.
*
***************************************************************************/

#ifndef GRAB_BAG_H
#define GRAB_BAG_H

#include <intsafe.h>  // For the safe integer types; UINT32 etc.

// Validation macros
#define VALID_OBJECT_POINTER(p)     ((p) && *(void**)(p))
#define VALID_HANDLE(h)             ((h) && ((h) != INVALID_HANDLE_VALUE))
#define VALID_FLAGS(f, ValidFlags)  (!((f) & ~(ValidFlags)))
#define SINGLE_FLAG_SET(f)          ((f) && !((f) & ((f)-1)))

// Random helper macros
#define MAKEBOOL(x)                 (BOOL((x) != 0))
#define HRFROMP(p)                  ((p) ? S_OK : E_OUTOFMEMORY)
#define HRFROMHANDLE(h)             (VALID_HANDLE(h) ? S_OK : E_OUTOFMEMORY)
#define XOR(a, b)                   (!(a) != !(b))
#define ALIGNED(a, b)               ((a) % (b) == 0)
#define IMPLIES(p, q)               ((!p) || (q))   // Logical implication: p => q

// Define countof() if necessary (better version at http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx)
#ifndef countof
    #ifdef _countof
        #define countof _countof
    #else
        #define countof(a) (sizeof(a) / sizeof *(a))
        template <typename T, size_t N> char (&_countof_helper(T (&array)[N]))[N];
        #define countof(array) (sizeof(_countof_helper(array)))
    #endif
#endif // #ifndef countof

// Define typeof() for some basic types
#ifndef typeof
    #define typeof(x) typeof_c<sizeof(*typeof_f(x))>::V
    #define REGISTER_TYPEOF(N,T) template<> struct typeof_c<N> {typedef wrap<T>::U V;}; char (*typeof_f(const wrap<T>::U &))[N]
    template<typename T> struct wrap {typedef T U;};
    template<int N> struct typeof_c;
    REGISTER_TYPEOF(1, char);
    REGISTER_TYPEOF(2, unsigned char);
    REGISTER_TYPEOF(3, short);
    REGISTER_TYPEOF(4, unsigned short);
    REGISTER_TYPEOF(5, int);
    REGISTER_TYPEOF(6, unsigned int);
    REGISTER_TYPEOF(7, long);
    REGISTER_TYPEOF(8, unsigned long);
    REGISTER_TYPEOF(9, float);
    REGISTER_TYPEOF(10, double);
    REGISTER_TYPEOF(11, long double);
    REGISTER_TYPEOF(12, char*);
    REGISTER_TYPEOF(13, unsigned char*);
    REGISTER_TYPEOF(14, short*);
    REGISTER_TYPEOF(15, unsigned short*);
    REGISTER_TYPEOF(16, int*);
    REGISTER_TYPEOF(17, unsigned int*);
    REGISTER_TYPEOF(18, long*);
    REGISTER_TYPEOF(19, unsigned long*);
    REGISTER_TYPEOF(20, float*);
    REGISTER_TYPEOF(21, double*);
    REGISTER_TYPEOF(22, long double*);
    REGISTER_TYPEOF(23, void*);
#endif // #ifndef typeof

// Local version of C_ASSERT (avoids definition clashes with the standard one)
#define GRAB_BAG_C_ASSERT(x) typedef char __compile_time_check__[(x) ? 1 : -1]

// Macro used to to display GUIDs more easily, as in:
// printf("GUID=" PRINTF_GUID_FORMAT "\n", PRINTF_GUID_FIELDS(guid));
#define PRINTF_GUID_FORMAT "{%8.8lX-%4.4X-%4.4X-%2.2X%2.2X-%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X}"
#define PRINTF_GUID_FIELDS(g) (g).Data1, (g).Data2, (g).Data3, (g).Data4[0], (g).Data4[1], (g).Data4[2], (g).Data4[3], (g).Data4[4], (g).Data4[5], (g).Data4[6], (g).Data4[7]


// Returns the absolute value of a numerical expression without evaluating it twice
template<typename T> inline T Abs(const T n) {return n >= 0 ? n : -n;}

// Returns the higher of two numerical expressions without evaluating them twice
template<typename T> inline T Max(const T a, const T b) {return a > b ? a : b;}

// Returns the lower of two numerical expressions without evaluating them twice
template<typename T> inline T Min(const T a, const T b) {return a < b ? a : b;}

// Returns the number of bits set in the given unsigned number
template <typename T> UINT32 CountBits(T x)
{
    UINT32 bitCount = 0;
    while (x) {++bitCount; x &= (x-1);}
    return bitCount;
}

// Define 64-bit Interlocked* functions as they are not available on XP
#if defined(_X86_) && _WIN32_WINNT < 0x0502
    inline LONGLONG InterlockedCompareExchange64(LONGLONG volatile* Destination, LONGLONG Exchange, LONGLONG Comperand)
    {
        __asm
        {
            mov esi, [Destination]
            mov ebx, dword ptr [Exchange]
            mov ecx, dword ptr [Exchange + 4]
            mov eax, dword ptr [Comperand]
            mov edx, dword ptr [Comperand + 4]
            lock cmpxchg8b [esi]
        }
    }
    inline LONGLONG InterlockedIncrement64(LONGLONG volatile* Addend)
    {
        LONGLONG Old;
        do Old = *Addend;
        while (InterlockedCompareExchange64(Addend, Old + 1, Old) != Old);
        return Old + 1;
    }
    inline LONGLONG InterlockedDecrement64(LONGLONG volatile* Addend)
    {
        LONGLONG Old;
        do Old = *Addend;
        while (InterlockedCompareExchange64(Addend, Old - 1, Old) != Old);
        return Old - 1;
    }
    inline LONGLONG InterlockedExchange64(LONGLONG volatile* Target, LONGLONG Value)
    {
        LONGLONG Old;
        do Old = *Target;
        while (InterlockedCompareExchange64(Target, Value, Old) != Old);
        return Old;
    }
#endif // defined(_X86_) && _WIN32_WINNT < 0x0502


// Typesafe threadsafe InterlockedIncrement/Decrement wrappers
template<typename T> inline T InterlockedIncrement(volatile T* pAddend)
{
    GRAB_BAG_C_ASSERT(sizeof T == sizeof LONG);
    return T(InterlockedIncrement(PLONG(pAddend)));
}
template<typename T> inline T InterlockedDecrement(volatile T* pAddend)
{
    GRAB_BAG_C_ASSERT(sizeof T == sizeof LONG);
    return T(InterlockedDecrement(PLONG(pAddend)));
}

// 64-bit specializations
#if defined(_X86_) && _WIN32_WINNT < 0x0502
    inline  INT64 InterlockedIncrement(volatile  INT64* pAddend) {return  INT64(InterlockedIncrement64(PLONGLONG(pAddend)));}
    inline  INT64 InterlockedDecrement(volatile  INT64* pAddend) {return  INT64(InterlockedDecrement64(PLONGLONG(pAddend)));}
    inline UINT64 InterlockedIncrement(volatile UINT64* pAddend) {return UINT64(InterlockedIncrement64(PLONGLONG(pAddend)));}
    inline UINT64 InterlockedDecrement(volatile UINT64* pAddend) {return UINT64(InterlockedDecrement64(PLONGLONG(pAddend)));}
#endif


// Typesafe threadsafe memory assignment and inspection
template<typename T> inline void AtomicAssign(T* pMemory, T value)
{
    GRAB_BAG_C_ASSERT(sizeof T == sizeof LONG);
    InterlockedExchange(PLONG(pMemory), LONG(value));
}
template<typename T> inline T AtomicRead(const T& memory)
{
    GRAB_BAG_C_ASSERT(sizeof T == sizeof LONG);
    return T(InterlockedCompareExchange(PLONG(&memory), -1, -1));
}
// 64-bit specializations
inline void AtomicAssign( INT64* pMemory,  INT64 value) {InterlockedExchange64(PLONGLONG(pMemory), LONGLONG(value));}
inline void AtomicAssign(UINT64* pMemory, UINT64 value) {InterlockedExchange64(PLONGLONG(pMemory), LONGLONG(value));}
inline  INT64 AtomicRead(const  INT64& memory) {return  INT64(InterlockedCompareExchange64(PLONGLONG(&memory), -1, -1));}
inline UINT64 AtomicRead(const UINT64& memory) {return UINT64(InterlockedCompareExchange64(PLONGLONG(&memory), -1, -1));}
#ifdef _WIN64
    // 64-bit pointer specializations
    inline void AtomicAssign(PVOID* pMemory, PVOID value) {InterlockedExchange64(PLONGLONG(pMemory), LONGLONG(value));}
    inline PVOID AtomicRead(const PVOID& memory) {return PVOID(InterlockedCompareExchange64(PLONGLONG(&memory), -1, -1));}
#else
    // 32-bit pointer specializations
    inline void AtomicAssign(PVOID* pMemory, PVOID value) {InterlockedExchange(PLONG(pMemory), LONG(value));}
    inline PVOID AtomicRead(const PVOID& memory) {return PVOID(InterlockedCompareExchange(PLONG(&memory), -1, -1));}
#endif


// QueryPerformance function wrappers
inline UINT64 GetPerformanceCounter()   {UINT64 n; QueryPerformanceCounter  (PLARGE_INTEGER(&n)); return n;}
inline UINT64 GetPerformanceFrequency() {UINT64 n; QueryPerformanceFrequency(PLARGE_INTEGER(&n)); return n;}

// Store the timer frequency to avoid multiple calls to GetPerformanceFrequency
extern const __declspec(selectany) UINT64 g_qpcTicksPerMs = GetPerformanceFrequency() / 1000;


// Template functions for releasing objects and handles safely
template<typename T> inline void RELEASE(T*& p)
{
    if (p)
    {
        p->Release();
        p = NULL;
    }
}
inline void CLOSE_HANDLE(HANDLE& h)
{
    if (VALID_HANDLE(h))
    {
        if (!CloseHandle(h))
        {
            #if defined(TRACER_H) && defined(USE_TRACER)
                TRACE(ERROR, "Failed to close handle 0x%lX: error=%ld", h, GetLastError());
                BREAK();
            #endif
        }
        h = 0;
    }
}


#endif // GRAB_BAG_H
