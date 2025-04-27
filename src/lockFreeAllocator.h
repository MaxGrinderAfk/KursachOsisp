#ifndef LOCK_FREE_ALLOCATOR_H
#define LOCK_FREE_ALLOCATOR_H

#include "libs.h"

class SubAllocator {
public:
    static constexpr std::size_t BLOCK_SIZE = 64;
    static constexpr std::size_t INITIAL_BLOCK_COUNT = 1024;
    static constexpr int MAX_RETRY_ATTEMPTS = 100;

    struct Block {
        std::atomic<Block*> next;
    };

    static SubAllocator& instance();
    void* allocate();
    void deallocate(void* ptr);

private:
    alignas(Block) uint8_t initial_memory_[BLOCK_SIZE * INITIAL_BLOCK_COUNT];
    std::atomic<Block*> free_list_head_;
    std::vector<void*> additional_pools_;
    std::mutex expansion_mutex_; 

    SubAllocator();
    ~SubAllocator();
    
    SubAllocator(const SubAllocator&) = delete;
    SubAllocator& operator=(const SubAllocator&) = delete;
    SubAllocator(SubAllocator&&) = delete;
    SubAllocator& operator=(SubAllocator&&) = delete;

    void initialize_pool(void* memory, std::size_t block_count);
    void expand_pool();
};

#endif