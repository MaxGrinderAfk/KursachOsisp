#ifndef LOCK_FREE_ALLOCATOR_POOL_H
#define LOCK_FREE_ALLOCATOR_POOL_H

#include "lockFreeAllocator.h"

template <typename T>
class SubAllocatorPool {
public:
    static SubAllocator& instance();
};

#include "lockFreeAllocPool.cpp"

#endif