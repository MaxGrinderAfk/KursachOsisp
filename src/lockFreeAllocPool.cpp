#ifndef LOCK_FREE_ALLOCATOR_POOL_CPP
#define LOCK_FREE_ALLOCATOR_POOL_CPP

#include "lockFreeAllocPool.h"

template <typename T>
SubAllocator& SubAllocatorPool<T>::instance() {
    return SubAllocator::instance(sizeof(T), alignof(T));
}

#endif
