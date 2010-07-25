/***************************************************************************
*
* File:     errors.h
* Content:  Functions used to map various types of numeric error code to
*           human-readable error names and descriptions.
*
***************************************************************************/

#ifndef ERRORS_H
#define ERRORS_H

#include <wtypes.h>      // For LPCSTR, HRESULT etc.
#include <specstrings.h> // For __out_ecount_opt

// Use a default component name prefix if none was specified
#ifndef TRACE_COMPONENT
    #pragma message("Warning: TRACE_COMPONENT not defined; using default names.")
    #define TRACE_COMPONENT Debug
#endif

// Redefine our function names based on the component name prefix; allows
// multiple libraries to use errors.cpp without confusing each other
#define _fix(x) x
#define ComponentErrorName _fix(TRACE_COMPONENT) ## ErrorName
#define ComponentErrorDescription _fix(TRACE_COMPONENT) ## ErrorDescription
#define ComponentErrorNameAndDescription _fix(TRACE_COMPONENT) ## ErrorNameAndDescription

// Function declarations
LPCSTR ComponentErrorName(HRESULT hr);
LPCSTR ComponentErrorDescription(HRESULT hr);
HRESULT ComponentErrorNameAndDescription(HRESULT hr,
                                         __out_ecount_opt(cchName) LPSTR pszName,
                                         size_t cchName,
                                         __out_ecount_opt(cchDesc) LPSTR pszDesc,
                                         size_t cchDesc);

#endif // ERRORS_H
