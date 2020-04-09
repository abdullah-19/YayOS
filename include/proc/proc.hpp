#ifndef __PROC_HPP_INCLUDED__
#define __PROC_HPP_INLCUDED__

#include <spinlock.hpp>
#include <state.hpp>
#include <timer.hpp>
#include <utils.hpp>

namespace proc {

    typedef Uint64 Pid;
    const Pid pidCount = 65536;
    typedef void (*EntryPoint)();

    struct Process {
        ProcessState state;
        Pid pid;
        Process* next;
        Process* prev;
        Uint64 padding[3];
    };

    static_assert(sizeof(Process) % 64 == 0);


    class ProcessManager {
        static bool initialized;
        static Process* schedListHead;
        static Process* processData;
        static Uint64* pidBitmap;
        static Uint64 pidBitmapSize;
        static Uint64 lastCheckedIndex;
        static lock::Spinlock modifierLock;
        static bool unlockSpinlock;

        static Pid pidAlloc();
        static void freePid(Pid pid);

    public:
        static drivers::Timer* timer;
        static bool yieldFlag;
        
        INLINE static bool isInitilaized() { return initialized; }
        static void schedule(SchedulerIntFrame* frame);
        static void yield();
        static void init(drivers::Timer* timer);
        static Process* newProc();
        static bool addToRunList(Process *proc);
        static bool suspendFromRunList(Pid pid);
    };

}; // namespace proc

#endif