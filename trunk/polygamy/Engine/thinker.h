// thinker.h

#pragma once
#include "threading.h"


class Thinker : private ThreadBase
{
public:

    HRESULT initialize();
    void terminate();

private:

    void ThreadProc();
};
