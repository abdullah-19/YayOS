#ifndef __SYSCALLS_HPP_INCLUDED__
#define __SYSCALLS_HPP_INCLUDED__

#include <utils.hpp>

namespace proc {
    [[noreturn]] void sysExit();
    int64_t sysHelloWorld();
}; // namespace proc

#endif