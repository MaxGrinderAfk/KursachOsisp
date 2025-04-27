#include "lockFreeAllocator.h"

SubAllocator& SubAllocator::instance() {
    static SubAllocator allocator;
    return allocator;
}

SubAllocator::SubAllocator() : free_list_head_(nullptr) {
    initialize_pool(initial_memory_, INITIAL_BLOCK_COUNT);
}

SubAllocator::~SubAllocator() {
    for (void* pool : additional_pools_) {
        ::operator delete(pool);
    }
}

void* SubAllocator::allocate() {
    Block* head = free_list_head_.load(std::memory_order_acquire);
    int retryCount = 0;
    
    while (head && retryCount < MAX_RETRY_ATTEMPTS) {
        Block* next = head->next.load(std::memory_order_relaxed);
        if (free_list_head_.compare_exchange_weak(head, next, std::memory_order_acquire, std::memory_order_relaxed)) {
            return reinterpret_cast<void*>(head);
        }
        retryCount++;
        
        if (retryCount % 10 == 0) {
            std::this_thread::yield();
        }
    }

    expand_pool();

    head = free_list_head_.load(std::memory_order_acquire);
    if (!head) {
        throw std::bad_alloc();
    }
    
    retryCount = 0;
    while (head && retryCount < MAX_RETRY_ATTEMPTS) {
        Block* next = head->next.load(std::memory_order_relaxed);
        if (free_list_head_.compare_exchange_weak(head, next, std::memory_order_acquire, std::memory_order_relaxed)) {
            return reinterpret_cast<void*>(head);
        }
        retryCount++;
        
        if (retryCount % 10 == 0) {
            std::this_thread::yield();
        }
    }

    throw std::bad_alloc();
}

void SubAllocator::deallocate(void* ptr) {
    if (!ptr) return;

    Block* block = reinterpret_cast<Block*>(ptr);
    Block* head = free_list_head_.load(std::memory_order_acquire);
    int retryCount = 0;
    
    do {
        block->next.store(head, std::memory_order_relaxed);
        
        if (retryCount++ > MAX_RETRY_ATTEMPTS) {
            std::this_thread::yield();
            retryCount = 0;
            head = free_list_head_.load(std::memory_order_acquire);
        }
    } while (!free_list_head_.compare_exchange_weak(head, block, std::memory_order_release, std::memory_order_relaxed));
}

void SubAllocator::initialize_pool(void* memory, std::size_t block_count) {
    Block* prev = nullptr;
    for (std::size_t i = 0; i < block_count; ++i) {
        Block* block = reinterpret_cast<Block*>(static_cast<uint8_t*>(memory) + i * BLOCK_SIZE);
        block->next.store(prev, std::memory_order_relaxed);
        prev = block;
    }
    free_list_head_.store(prev, std::memory_order_release);
}

void SubAllocator::expand_pool() {
    std::lock_guard<std::mutex> lock(expansion_mutex_);

    if (free_list_head_.load(std::memory_order_acquire) != nullptr) {
        return;
    }

    constexpr std::size_t new_block_count = INITIAL_BLOCK_COUNT; 
    void* new_memory = ::operator new(BLOCK_SIZE * new_block_count);

    additional_pools_.push_back(new_memory);

    Block* prev = nullptr;
    for (std::size_t i = 0; i < new_block_count; ++i) {
        Block* block = reinterpret_cast<Block*>(static_cast<uint8_t*>(new_memory) + i * BLOCK_SIZE);
        block->next.store(prev, std::memory_order_relaxed);
        prev = block;
    }

    Block* old_head = free_list_head_.load(std::memory_order_acquire);
    int retryCount = 0;
    
    do {
        prev->next.store(old_head, std::memory_order_relaxed);
        
        if (retryCount++ > MAX_RETRY_ATTEMPTS) {
            std::this_thread::yield();
            retryCount = 0;
            old_head = free_list_head_.load(std::memory_order_acquire);
        }
    } while (!free_list_head_.compare_exchange_weak(old_head, prev, std::memory_order_release, std::memory_order_relaxed));
}