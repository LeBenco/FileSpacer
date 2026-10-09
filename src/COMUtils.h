#pragma once
#include <common.h>

#include "main.h"
#include <Unknwn.h>

namespace filespacer {

class UnknownImpl : public IUnknown {
public:
    virtual ~UnknownImpl() = default;

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID id, void **obj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

private:
    long refCount = 1;
};

class StoppableThread : public UnknownImpl {
public:
    StoppableThread();
    virtual ~StoppableThread();
    void start(); // do not call multiple times!
    void stop();

protected:
    virtual void run() = 0;

    bool isStopped();
    HANDLE stopEvent;
    SRWLOCK stopLock = SRWLOCK_INIT; // thread will not be stopped while held

private:
    HANDLE thread = nullptr;

    static DWORD WINAPI threadProc(void *);
};

} // namespace
