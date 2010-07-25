// shared.h

#pragma once

#include <SDKDDKVer.h>
// This defines the highest available Windows platform.  To target a prior
// platform, include WinSDKVer.h and set the appropriate _WIN32_WINNT macro
// before including SDKDDKVer.h.

#include <stdio.h>      // For printf(), gets_s() etc.
#include <stdlib.h>     // For atoi(), srand(), rand()
#include <memory.h>     // For memcpy()
#include <string.h>     // For memmove()
#include <ctype.h>      // For toupper()
#include <time.h>       // For time(), used to seed the random number generator
#include <intsafe.h>    // For UINT64
#include <windows.h>    // For QueryPerformance*
#include <strsafe.h>    // For StringCchPrintf*

// Shared code
#include "utils.h"

// Some portability macros
#ifndef FORCEINLINE
    #if VISUAL_STUDIO_FIXME
        #define FORCEINLINE __forceinline
    #elif GCC_FIXME
        #define FORCEINLINE inline // FIXME: See if GCC has a way to force inlining
    #else
        #define FORCEINLINE inline // FIXME: See if GCC
    #endif
#endif

// Default debug settings (can override in registry under HKCU\Software\Dugan\Polygamy)
#define DBG_COMPONENT           Polygamy
#define DBG_DEFAULT_LEVEL       (TraceType_Error|TraceType_Warning)
#define DBG_DEFAULT_BREAK_LEVEL (TraceType_Error)
#include "debughlp.h"

// Global configuration (overrules per-module defaults)
#define RANDOMIZE 0               // Use nondeterministic computer move sequences
#define CONSISTENCY_CHECKS 0      // Internal correctness checks [FIXME: unused right now]
#define USE_GAMENODE_HEAP 0       // Use HeapAlloc() for game nodes rather than new() [FIXME - SLOWER - remove?]

// FIXME: Tidy this up if ever we enable it
#if USE_GAMENODE_HEAP
    extern HANDLE g_gamenode_heap;
    extern unsigned __int64 g_allocations;
#endif

// Minimax algorithm tuning
#define DEFAULT_MAXIMUM_DEPTH 10    // Default maximum search depth if unspecified by user
#define DEFAULT_ANALYSIS_TIME 5     // Default position analysis time if unspecified by user
#define MINIMAX_STATISTICS 1        // Display number of nodes examined, beta cutoffs, etc.
#define MINIMAX_TRACE 0             // Display minimax algorithm progress on-screen

// Othello-specific constants
#define OTH_DIMENSION 8             // Default board size
#define OTH_GAME_STATE_LIST 0       // Slightly faster (does all work on a single board)
#define OTH_DISPLAY_EVALUATION 0    // Show position evaluation details

// Tic-tac-toe-specific constants
#define TTT_DIMENSION 3             // Default board size

// Connect4-specific constants
#define CONNECT4_COLUMNS 7          // Default board width
#define CONNECT4_ROWS 6             // Default board height

// Ataxx-specific constants
#define ATAXX_COLUMNS 7             // Default board width
#define ATAXX_ROWS 7                // Default board height

// Kalah-specific constants
#define KALAH_PITS 6                // Numbers of pits (houses) per side
#define KALAH_SEEDS 4               // Number of seeds initially in each pit
