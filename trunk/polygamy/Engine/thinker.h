// thinker.h

#pragma once
#include "threader.h"


class Thinker : private ThreadBase
{
public:

    HRESULT initialize();
    void terminate();

private:

    void ThreadProc();
};
