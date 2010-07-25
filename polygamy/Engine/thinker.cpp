// thinker.cpp

#include "shared.h"   // Precompiled header; obligatory
#include "thinker.h"  // Our public interface


HRESULT Thinker::initialize()
{
    HRESULT hr = ThreadBase::Initialize();

    StartThread();

    return hr;
}


void Thinker::terminate()
{
    StopThread();
}


void Thinker::ThreadProc()
{
    // FIXME: to do...
}
