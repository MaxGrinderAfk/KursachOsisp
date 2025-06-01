#ifndef LOCK_FREE_ALLOCATOR_H
#define LOCK_FREE_ALLOCATOR_H

#include "libs.h"

class SubAllocator {
public:
    struct Block {
        std::atomic<Block*> next;
    };

    static constexpr std::size_t INITIAL_BLOCK_COUNT = 1024;
    static constexpr int MAX_RETRY_ATTEMPTS = 100;

    static SubAllocator& instance(std::size_t block_size = 64 /*+ sizeof(Block)*/, 
                                  std::size_t alignment = alignof(std::max_align_t));
    void* allocate();
    void deallocate(void* ptr);

private:
    std::size_t block_size_;
    std::size_t alignment_;
    uint8_t* initial_memory_;
    std::atomic<Block*> free_list_head_;
    std::vector<void*> additional_pools_;
    std::mutex expansion_mutex_; 

    SubAllocator(std::size_t block_size, std::size_t alignment);
    ~SubAllocator();
    
    SubAllocator(const SubAllocator&) = delete;
    SubAllocator& operator=(const SubAllocator&) = delete;
    SubAllocator(SubAllocator&&) = delete;
    SubAllocator& operator=(SubAllocator&&) = delete;

    void initialize_pool(void* memory, std::size_t block_count);
    void expand_pool();
};

#endif