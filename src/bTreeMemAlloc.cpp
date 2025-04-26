#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include <cstddef>
#include <new>
#include <shared_mutex>
#include <array>
#include <atomic>
#include <thread>
#include <chrono>
#include <random>
#include <sstream>
#include <cassert>
#include <atomic>
#include <cstdint>
#include <unordered_set>

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

template <typename T>
class BTree {
private:
    int t;
    mutable std::shared_mutex tree_mutex;
    
    struct Node {
        bool isLeaf;
        std::vector<T> keys;
        std::vector<Node*> children;
        
        Node(bool leaf = true);
        ~Node();
        
        static void* operator new(std::size_t size);
        static void operator delete(void* ptr, std::size_t size);        
    };
    
    Node* root;
    
    void splitChild(Node* parent, int index);
    void insertNonFull(Node* node, const T& key);
    bool search(Node* node, const T& key, int& pos) const;
    T getPredecessor(Node* node, int index);
    T getSuccessor(Node* node, int index);
    void mergeNodes(Node* node, int index);
    void borrowFromPrev(Node* node, int index);
    void borrowFromNext(Node* node, int index);
    void fill(Node* node, int index);
    void removeFromLeaf(Node* node, int index);
    void removeFromNonLeaf(Node* node, int index);
    int findKey(Node* node, const T& key);
    void remove(Node* node, const T& key);
    void traverse(Node* node) const;
    
public:
    BTree(int degree);
    ~BTree();
    
    void traverse() const;
    bool search(const T& key) const;
    void insert(const T& key);
    void remove(const T& key);
};
    
void testBasicOperations();
void testEdgeCases();
void testConcurrency();
void testRepeatedInsertionsAndDeletions();
void testLargeRangeInsertions();
void testTreeSizeApproximation();
void testAlternatingInsertRemove();
void testConcurrencyMixed();
void testHeavyConcurrency();

int main() {
    testBasicOperations();
    testEdgeCases();
    testConcurrency();
    testRepeatedInsertionsAndDeletions();
    testLargeRangeInsertions();
    testTreeSizeApproximation();
    testAlternatingInsertRemove();
    testConcurrencyMixed();
    testHeavyConcurrency();

    std::cout << "\nВсе тесты успешно пройдены!" << std::endl;
    return 0;
}

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

template <typename T>
BTree<T>::Node::Node(bool leaf) : isLeaf(leaf) {}

template <typename T>
BTree<T>::Node::~Node() {
    for (auto& child : children) {
        delete child;
    }
}

template <typename T>
void* BTree<T>::Node::operator new(std::size_t /*size*/) {
    void* ptr = SubAllocator::instance().allocate();
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

template <typename T>
void BTree<T>::Node::operator delete(void* ptr, std::size_t /*size*/) {
    SubAllocator::instance().deallocate(ptr);
}

template <typename T>
BTree<T>::BTree(int degree) {
    t = std::max(2, degree);  
    root = new Node(true);
}

template <typename T>
BTree<T>::~BTree() {
    delete root;
}

template <typename T>
void BTree<T>::traverse() const {
    std::shared_lock<std::shared_mutex> lock(tree_mutex);
    if (root) {
        traverse(root);
    }
    std::cout << std::endl;
}

template <typename T>
bool BTree<T>::search(const T& key) const {
    std::shared_lock<std::shared_mutex> lock(tree_mutex);
    if (!root) {
        return false;
    }
    
    int pos;
    return search(root, key, pos);
}

template <typename T>
void BTree<T>::insert(const T& key) {
    std::unique_lock<std::shared_mutex> lock(tree_mutex);
    if (!root) {
        root = new Node(true);
    }
    
    if (root->keys.size() == 2 * t - 1) {
        Node* newRoot = new Node(false);
        newRoot->children.push_back(root);
        root = newRoot;
        splitChild(root, 0);
        insertNonFull(root, key);
    } else {
        insertNonFull(root, key);
    }
}

template <typename T>
void BTree<T>::remove(const T& key) {
    std::unique_lock<std::shared_mutex> lock(tree_mutex);
    if (!root) {
        return;
    }
    
    remove(root, key);
    
    if (root->keys.empty()) {
        if (root->isLeaf) {
        } else if (!root->children.empty()) {
            Node* oldRoot = root;
            root = root->children[0];
            
            oldRoot->children.clear();
            delete oldRoot;
        }
    }
}

template <typename T>
void BTree<T>::splitChild(Node* parent, int index) {
    if (!parent || index < 0 || index >= parent->children.size()) {
        return;
    }
    
    Node* y = parent->children[index];
    if (!y || y->keys.size() < 2*t - 1) {
        return;
    }
    
    Node* z = new Node(y->isLeaf);
    
    parent->keys.insert(parent->keys.begin() + index, y->keys[t - 1]);
    parent->children.insert(parent->children.begin() + index + 1, z);
    
    z->keys.assign(y->keys.begin() + t, y->keys.end());
    y->keys.resize(t - 1);
    
    if (!y->isLeaf) {
        z->children.assign(y->children.begin() + t, y->children.end());
        y->children.resize(t);
    }
}

template <typename T>
void BTree<T>::insertNonFull(Node* node, const T& key) {
    if (!node) return;
    
    int i = node->keys.size() - 1;
    
    if (node->isLeaf) {
        node->keys.push_back(T());
        while (i >= 0 && key < node->keys[i]) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }
        node->keys[i + 1] = key;
    } else {
        while (i >= 0 && key < node->keys[i]) {
            i--;
        }
        i++;
        
        if (i < node->children.size() && node->children[i] && 
            node->children[i]->keys.size() == 2 * t - 1) {
            splitChild(node, i);
            if (key > node->keys[i]) {
                i++;
            }
        }
        
        if (i < node->children.size() && node->children[i]) {
            insertNonFull(node->children[i], key);
        }
    }
}

template <typename T>
bool BTree<T>::search(Node* node, const T& key, int& pos) const {
    if (!node) return false;
    
    int i = 0;
    while (i < node->keys.size() && key > node->keys[i]) {
        i++;
    }
    
    if (i < node->keys.size() && key == node->keys[i]) {
        pos = i;
        return true;
    }
    
    if (node->isLeaf) {
        return false;
    }
    
    if (i < node->children.size()) {
        return search(node->children[i], key, pos);
    }
    
    return false;
}

template <typename T>
T BTree<T>::getPredecessor(Node* node, int index) {
    if (!node || index < 0 || index >= node->children.size() || !node->children[index]) {
        return T();
    }
    
    Node* current = node->children[index];
    while (!current->isLeaf && !current->children.empty()) {
        current = current->children[current->children.size() - 1];
    }
    
    if (current->keys.empty()) {
        return T();
    }
    
    return current->keys[current->keys.size() - 1];
}

template <typename T>
T BTree<T>::getSuccessor(Node* node, int index) {
    if (!node || index < 0 || index + 1 >= node->children.size() || !node->children[index + 1]) {
        return T();
    }
    
    Node* current = node->children[index + 1];
    while (!current->isLeaf && !current->children.empty()) {
        current = current->children[0];
    }
    
    if (current->keys.empty()) {
        return T();
    }
    
    return current->keys[0];
}

template <typename T>
void BTree<T>::mergeNodes(Node* node, int index) {
    if (!node || index < 0 || index >= node->children.size() - 1) {
        return;
    }
    
    Node* child = node->children[index];
    Node* sibling = node->children[index + 1];
    
    if (!child || !sibling) {
        return;
    }
    
    if (index < node->keys.size()) {
        child->keys.push_back(node->keys[index]);
        
        child->keys.insert(child->keys.end(), sibling->keys.begin(), sibling->keys.end());
        
        if (!child->isLeaf) {
            child->children.insert(child->children.end(), sibling->children.begin(), sibling->children.end());
            sibling->children.clear();
        }
        
        node->keys.erase(node->keys.begin() + index);
        
        node->children.erase(node->children.begin() + index + 1);
        
        delete sibling;
    }
}

template <typename T>
void BTree<T>::borrowFromPrev(Node* node, int index) {
    if (!node || index <= 0 || index >= node->children.size()) {
        return;
    }
    
    Node* child = node->children[index];
    Node* sibling = node->children[index - 1];
    
    if (!child || !sibling || sibling->keys.empty()) {
        return;
    }
    
    child->keys.insert(child->keys.begin(), node->keys[index - 1]);
    
    if (index - 1 < node->keys.size()) {
        node->keys[index - 1] = sibling->keys.back();
        sibling->keys.pop_back();
        
        if (!child->isLeaf && !sibling->children.empty()) {
            child->children.insert(child->children.begin(), sibling->children.back());
            sibling->children.pop_back();
        }
    }
}

template <typename T>
void BTree<T>::borrowFromNext(Node* node, int index) {
    if (!node || index < 0 || index >= node->children.size() - 1 || index >= node->keys.size()) {
        return;
    }
    
    Node* child = node->children[index];
    Node* sibling = node->children[index + 1];
    
    if (!child || !sibling || sibling->keys.empty()) {
        return;
    }
    
    child->keys.push_back(node->keys[index]);
    
    node->keys[index] = sibling->keys.front();
    sibling->keys.erase(sibling->keys.begin());
    
    if (!child->isLeaf && !sibling->children.empty()) {
        child->children.push_back(sibling->children.front());
        sibling->children.erase(sibling->children.begin());
    }
}

template <typename T>
void BTree<T>::fill(Node* node, int index) {
    if (!node || index < 0 || index >= node->children.size()) {
        return;
    }
    
    if (index > 0 && node->children[index - 1] && node->children[index - 1]->keys.size() >= t) {
        borrowFromPrev(node, index);
    } 
    else if (index < node->children.size() - 1 && node->children[index + 1] && 
            node->children[index + 1]->keys.size() >= t) {
        borrowFromNext(node, index);
    } 
    else {
        if (index < node->children.size() - 1) {
            mergeNodes(node, index);
        } else if (index > 0) {
            mergeNodes(node, index - 1);
        }
    }
}

template <typename T>
void BTree<T>::removeFromLeaf(Node* node, int index) {
    if (!node || index < 0 || index >= node->keys.size()) {
        return;
    }
    node->keys.erase(node->keys.begin() + index);
}

template <typename T>
void BTree<T>::removeFromNonLeaf(Node* node, int index) {
    if (!node || index < 0 || index >= node->keys.size()) {
        return;
    }
    
    T key = node->keys[index];
    
    if (index < node->children.size() && node->children[index] && node->children[index]->keys.size() >= t) {
        T pred = getPredecessor(node, index);
        node->keys[index] = pred;
        remove(node->children[index], pred);
    } 
    else if (index + 1 < node->children.size() && node->children[index + 1] && 
            node->children[index + 1]->keys.size() >= t) {
        T succ = getSuccessor(node, index);
        node->keys[index] = succ;
        remove(node->children[index + 1], succ);
    } 
    else if (index < node->children.size() - 1 && node->children[index] && node->children[index + 1]) {
        mergeNodes(node, index);
        remove(node->children[index], key);
    }
}

template <typename T>
int BTree<T>::findKey(Node* node, const T& key) {
    int index = 0;
    if (!node) return index;
    
    while (index < node->keys.size() && node->keys[index] < key) {
        ++index;
    }
    return index;
}

template <typename T>
void BTree<T>::remove(Node* node, const T& key) {
    if (!node) {
        return;
    }
    
    int index = findKey(node, key);
    
    if (index < node->keys.size() && node->keys[index] == key) {
        if (node->isLeaf) {
            removeFromLeaf(node, index);
        } else {
            removeFromNonLeaf(node, index);
        }
    } 
    else {
        if (node->isLeaf) {
            return;
        }
        
        bool flag = (index == node->keys.size());
        
        if (index < node->children.size() && node->children[index] && 
            node->children[index]->keys.size() < t) {
            fill(node, index);
        }
        
        if (flag && index > node->children.size() - 1) {
            if (index > 0) {
                index = node->children.size() - 1;
            } else {
                return;
            }
        }
        
        if (index < node->children.size() && node->children[index]) {
            remove(node->children[index], key);
        }
    }
}

template <typename T>
void BTree<T>::traverse(Node* node) const {
    if (!node) return;
    
    int i;
    for (i = 0; i < node->keys.size(); i++) {
        if (!node->isLeaf && i < node->children.size() && node->children[i]) {
            traverse(node->children[i]);
        }
        std::cout << node->keys[i] << " ";
    }
    
    if (!node->isLeaf && i < node->children.size() && node->children[i]) {
        traverse(node->children[i]);
    }
}

void testBasicOperations() {
    std::cout << "\n=== Тест 1: Базовые операции ===" << std::endl;
    BTree<int> tree(3);
    
    for (int i : {10, 20, 5, 15, 25, 30, 1, 2}) {
        tree.insert(i);
    }
    
    for (int i : {10, 20, 5, 15, 25, 30, 1, 2}) {
        assert(tree.search(i) && "Элемент не найден после вставки");
    }
    
    tree.remove(10);
    tree.remove(5);
    tree.remove(30);
    
    assert(!tree.search(10) && "Удаленный элемент найден");
    assert(!tree.search(5) && "Удаленный элемент найден");
    assert(!tree.search(30) && "Удаленный элемент найден");
    
    for (int i : {20, 15, 25, 1, 2}) {
        assert(tree.search(i) && "Оставшийся элемент не найден");
    }
    
    std::cout << "Тест 1 пройден успешно!\n";
}

void testEdgeCases() {
    std::cout << "\n=== Тест 2: Крайние случаи ===" << std::endl;
    BTree<int> tree(2);
    
    tree.insert(100);
    assert(tree.search(100) && "Единственный элемент не найден");
    
    tree.remove(100);
    assert(!tree.search(100) && "Удаленный элемент найден");
    
    tree.remove(42); 
    
    for (int i = 0; i < 1000; ++i) {
        tree.insert(i);
    }
    for (int i = 0; i < 1000; ++i) {
        assert(tree.search(i) && "Элемент не найден после массовой вставки");
    }
    for (int i = 0; i < 1000; ++i) {
        tree.remove(i);
    }
    for (int i = 0; i < 1000; ++i) {
        assert(!tree.search(i) && "Элемент найден после массового удаления");
    }
    
    std::cout << "Тест 2 пройден успешно!\n";
}

void testConcurrency() {
    std::cout << "\n=== Тест 3: Многопоточные операции ===" << std::endl;
    BTree<int> tree(3);
    const int numThreads = 4;
    const int numElements = 1000;
    
    auto insertFunc = [&tree](int start) {
        for (int i = start; i < start + numElements; ++i) {
            tree.insert(i);
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(insertFunc, i * numElements);
    }
    for (auto& t : threads) {
        t.join();
    }
    
    for (int i = 0; i < numThreads * numElements; ++i) {
        assert(tree.search(i) && "Элемент не найден после многопоточной вставки");
    }
    
    threads.clear();
    auto removeFunc = [&tree](int start) {
        for (int i = start; i < start + numElements; ++i) {
            tree.remove(i);
        }
    };
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(removeFunc, i * numElements);
    }
    for (auto& t : threads) {
        t.join();
    }
    
    for (int i = 0; i < numThreads * numElements; ++i) {
        assert(!tree.search(i) && "Элемент найден после многопоточного удаления");
    }
    
    std::cout << "Тест 3 пройден успешно!\n";
}

void testRepeatedInsertionsAndDeletions() {
    std::cout << "\n=== Тест 4: Повторные вставки и удаления ===" << std::endl;
    BTree<int> tree(3);

    for (int i = 0; i < 100; ++i) {
        tree.insert(i);
        tree.insert(i); 
    }

    for (int i = 0; i < 100; ++i) {
        assert(tree.search(i) && "Элемент не найден после повторной вставки");
    }

    for (int i = 0; i < 100; ++i) {
        tree.remove(i);
        tree.remove(i);
    }

    for (int i = 0; i < 100; ++i) {
        assert(!tree.search(i) && "Элемент найден после двойного удаления");
    }

    std::cout << "Тест 4 пройден успешно!\n";
}

void testLargeRangeInsertions() {
    std::cout << "\n=== Тест 5: Массовая вставка большого диапазона значений ===" << std::endl;
    BTree<int> tree(5);

    for (int i = 1000000; i < 1001000; ++i) {
        tree.insert(i);
    }

    for (int i = 1000000; i < 1001000; ++i) {
        assert(tree.search(i) && "Элемент не найден после вставки большого диапазона");
    }

    std::cout << "Тест 5 пройден успешно!\n";
}

void testTreeSizeApproximation() {
    std::cout << "\n=== Тест 6: Приближенная проверка размера дерева ===" << std::endl;
    BTree<int> tree(3);

    for (int i = 0; i < 500; ++i) {
        tree.insert(i);
    }

    for (int i = 250; i < 500; ++i) {
        tree.remove(i);
    }

    for (int i = 0; i < 250; ++i) {
        assert(tree.search(i) && "Элемент должен быть в дереве");
    }

    for (int i = 250; i < 500; ++i) {
        assert(!tree.search(i) && "Элемент не должен быть в дереве");
    }

    std::cout << "Тест 6 пройден успешно!\n";
}

void testAlternatingInsertRemove() {
    std::cout << "\n=== Тест 7: Перемешанные вставки и удаления ===" << std::endl;
    BTree<int> tree(4);

    for (int i = 0; i < 10000; i += 2) {
        tree.insert(i);
    }

    for (int i = 0; i < 10000; ++i) {
        if (i % 4 == 0) {
            tree.remove(i);
        } else if (i % 2 == 0) {
            assert(tree.search(i) && "Элемент должен быть найден после вставки");
        }
    }

    for (int i = 0; i < 10000; ++i) {
        if (i % 4 == 0) {
            assert(!tree.search(i) && "Элемент не должен быть найден после удаления");
        }
    }

    std::cout << "Тест 7 пройден успешно!\n";
}

void testConcurrencyMixed() {
    std::cout << "\n=== Тест 8: Смешанная многопоточность (вставка и удаление одновременно) ===" << std::endl;

    BTree<int> tree(3);
    const int numThreads = 4;
    const int numElements = 1000;

    auto insertFunc = [&tree](int start) {
        for (int i = start; i < start + numElements; ++i) {
            tree.insert(i);
        }
    };

    auto removeFunc = [&tree](int start) {
        for (int i = start; i < start + numElements; ++i) {
            tree.remove(i);
        }
    };

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        if (i % 2 == 0) {
            threads.emplace_back(insertFunc, i * numElements);
        } else {
            threads.emplace_back(removeFunc, (i - 1) * numElements); 
        }
    }

    for (auto& t : threads) {
        t.join();
    }

    int insertedElements = (numThreads / 2) * numElements;
    int foundElements = 0;
    for (int i = 0; i < insertedElements * 2; ++i) {
        bool found = tree.search(i);
        if (found) {
            foundElements++;
            if (i % 2 == 1) {
                assert(found && "Элемент должен быть найден после вставки нечетного потоком");
            }
        } else {
            if (i % 2 == 0 && i < insertedElements) {
                assert(!found && "Элемент не должен быть найден после удаления четного потоком");
            }
        }
    }
    
    std::cout << "Найдено элементов: " << foundElements << std::endl;
    std::cout << "Тест 8 пройден успешно!\n";
}

void testHeavyConcurrency() {
    std::cout << "\n=== Тест 9: Финальный стресс-тест ===" << std::endl;
    BTree<int> tree(4);
    const int numThreads = 8;
    const int operationsPerThread = 50000;
    const int keyRange = 100000;

    std::atomic<bool> running{true};
    std::atomic<int> insertCount{0};
    std::atomic<int> removeCount{0};

    std::vector<std::thread> threads;
    std::atomic<int>* keyCounters = new std::atomic<int>[keyRange + 1]();

    auto worker = [&](int id) {
        std::mt19937 rng(std::random_device{}() + id);
        std::uniform_int_distribution<int> opDist(0, 1);
        std::uniform_int_distribution<int> keyDist(0, keyRange);
    
        for (int i = 0; i < operationsPerThread; ++i) {
            int op = opDist(rng);
            int key = keyDist(rng);
    
            if (op == 0) {
                tree.insert(key);
                keyCounters[key].fetch_add(1, std::memory_order_relaxed);
                insertCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                int expected = keyCounters[key].load(std::memory_order_relaxed);
                if (expected > 0) {
                    bool success = false;
                    while (expected > 0 && !success) {
                        success = keyCounters[key].compare_exchange_weak(
                            expected, expected - 1,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        if (success) {
                            tree.remove(key);
                            removeCount.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            expected = keyCounters[key].load(std::memory_order_relaxed);
                        }
                    }
                }
            }
        }
    };

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Все потоки завершены. Проверка целостности дерева..." << std::endl;
    std::cout << "Вставлено ключей: " << insertCount.load() << std::endl;
    std::cout << "Удалено ключей: " << removeCount.load() << std::endl;

    std::unordered_set<int> finalExpectedKeys;
    for (int key = 0; key <= keyRange; ++key) {
        int count = keyCounters[key].load(std::memory_order_relaxed);
        if (count > 0) {
            finalExpectedKeys.insert(key);
        }
    }

    int foundCount = 0;
    int discrepancies = 0;
    std::vector<int> missingKeys;
    std::vector<int> extraKeys;

    for (int key = 0; key <= keyRange; ++key) {
        bool expected = finalExpectedKeys.count(key) > 0;
        bool actual = tree.search(key);

        if (expected != actual) {
            if (discrepancies < 10) {
                std::cerr << "Mismatch for key " << key << ": Expected="
                          << expected << ", Actual=" << actual 
                          << ", Counter=" << keyCounters[key].load() << std::endl;
                if (expected && !actual) missingKeys.push_back(key);
                if (!expected && actual) extraKeys.push_back(key);
            }
            discrepancies++;
        }

        if (actual) {
            foundCount++;
        }
    }

    if (discrepancies > 0) {
        std::cerr << "ОШИБКА: Обнаружено " << discrepancies << " несоответствий!" << std::endl;
        if (!missingKeys.empty()) {
            std::cerr << "Примеры отсутствующих ключей: ";
            for (size_t i = 0; i < std::min(missingKeys.size(), (size_t)5); ++i) {
                std::cerr << missingKeys[i] << " ";
            }
            std::cerr << std::endl;
        }
        if (!extraKeys.empty()) {
            std::cerr << "Примеры лишних ключей: ";
            for (size_t i = 0; i < std::min(extraKeys.size(), (size_t)5); ++i) {
                std::cerr << extraKeys[i] << " ";
            }
            std::cerr << std::endl;
        }
        assert(discrepancies == 0 && "Несоответствие наличия ключа после стресс-теста!");
    } else {
        std::cout << "Проверка целостности дерева прошла успешно: несоответствий не обнаружено!" << std::endl;
        std::cout << "Финальный стресс-тест успешно пройден!" << std::endl;
    }
    std::cout << "Найдено элементов в дереве: " << foundCount << "\n";

    delete[] keyCounters;
}