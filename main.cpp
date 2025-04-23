#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include <cstddef>
#include <new>

class SubAllocator {
public:
    static SubAllocator& instance() {
        static SubAllocator instance;
        return instance;
    }

    void* allocate(std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!freeList.empty()) {
            void* ptr = freeList.back();
            freeList.pop_back();
            return ptr;
        }

        return ::operator new(size);
    }

    void deallocate(void* ptr, std::size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        freeList.push_back(ptr);
    }

    void releasePool() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto ptr : freeList) {
            ::operator delete(ptr);
        }
        freeList.clear();
    }

private:
    SubAllocator() {}
    ~SubAllocator() { releasePool(); }
    SubAllocator(const SubAllocator&) = delete;
    SubAllocator& operator=(const SubAllocator&) = delete;

    std::vector<void*> freeList;
    std::mutex mutex_;
};


template <typename T>
class BTree {
private:
    int t;
    
    struct Node {
        bool isLeaf;
        std::vector<T> keys;
        std::vector<Node*> children;
        
        Node(bool leaf = true) : isLeaf(leaf) {}
        
        static void* operator new(std::size_t size) {
            return SubAllocator::instance().allocate(size);
        }

        static void operator delete(void* ptr, std::size_t size) {
            SubAllocator::instance().deallocate(ptr, size);
        }

        ~Node() {
            for (auto& child : children) {
                delete child;
            }
        }
    };
    
    Node* root;
    
    void splitChild(Node* parent, int index) {
        Node* y = parent->children[index];
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
    
    void insertNonFull(Node* node, const T& key) {
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
            
            if (node->children[i]->keys.size() == 2 * t - 1) {
                splitChild(node, i);
                if (key > node->keys[i]) {
                    i++;
                }
            }
            
            insertNonFull(node->children[i], key);
        }
    }
    
    bool search(Node* node, const T& key, int& pos) const {
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
        
        return search(node->children[i], key, pos);
    }
    
    T getPredecessor(Node* node, int index) {
        Node* current = node->children[index];
        while (!current->isLeaf) {
            current = current->children[current->keys.size()];
        }
        return current->keys[current->keys.size() - 1];
    }
    
    T getSuccessor(Node* node, int index) {
        Node* current = node->children[index + 1];
        while (!current->isLeaf) {
            current = current->children[0];
        }
        return current->keys[0];
    }
    
    void mergeNodes(Node* node, int index) {
        Node* child = node->children[index];
        Node* sibling = node->children[index + 1];
        
        child->keys.push_back(node->keys[index]);
        
        child->keys.insert(child->keys.end(), sibling->keys.begin(), sibling->keys.end());
        
        if (!child->isLeaf) {
            child->children.insert(child->children.end(), sibling->children.begin(), sibling->children.end());
        }
        
        node->keys.erase(node->keys.begin() + index);
        node->children.erase(node->children.begin() + index + 1);
        
        delete sibling;
    }
    
    void borrowFromPrev(Node* node, int index) {
        Node* child = node->children[index];
        Node* sibling = node->children[index - 1];
        
        child->keys.insert(child->keys.begin(), node->keys[index - 1]);
        
        node->keys[index - 1] = sibling->keys.back();
        sibling->keys.pop_back();
        
        if (!child->isLeaf) {
            child->children.insert(child->children.begin(), sibling->children.back());
            sibling->children.pop_back();
        }
    }
    
    void borrowFromNext(Node* node, int index) {
        Node* child = node->children[index];
        Node* sibling = node->children[index + 1];
        
        child->keys.push_back(node->keys[index]);
        
        node->keys[index] = sibling->keys.front();
        sibling->keys.erase(sibling->keys.begin());
        
        if (!child->isLeaf) {
            child->children.push_back(sibling->children.front());
            sibling->children.erase(sibling->children.begin());
        }
    }
    
    void fill(Node* node, int index) {
        if (index != 0 && node->children[index - 1]->keys.size() >= t) {
            borrowFromPrev(node, index);
        } else if (index != node->keys.size() && node->children[index + 1]->keys.size() >= t) {
            borrowFromNext(node, index);
        } else {
            if (index != node->keys.size()) {
                mergeNodes(node, index);
            } else {
                mergeNodes(node, index - 1);
            }
        }
    }
    
    void removeFromLeaf(Node* node, int index) {
        node->keys.erase(node->keys.begin() + index);
    }
    
    void removeFromNonLeaf(Node* node, int index) {
        T key = node->keys[index];
        
        if (node->children[index]->keys.size() >= t) {
            T pred = getPredecessor(node, index);
            node->keys[index] = pred;
            remove(node->children[index], pred);
        } else if (node->children[index + 1]->keys.size() >= t) {
            T succ = getSuccessor(node, index);
            node->keys[index] = succ;
            remove(node->children[index + 1], succ);
        } else {
            mergeNodes(node, index);
            remove(node->children[index], key);
        }
    }
    
    int findKey(Node* node, const T& key) {
        int index = 0;
        while (index < node->keys.size() && node->keys[index] < key) {
            ++index;
        }
        return index;
    }
    
    void remove(Node* node, const T& key) {
        int index = findKey(node, key);
        
        if (index < node->keys.size() && node->keys[index] == key) {
            if (node->isLeaf) {
                removeFromLeaf(node, index);
            } else {
                removeFromNonLeaf(node, index);
            }
        } else {
            if (node->isLeaf) {
                return;
            }
            
            bool flag = (index == node->keys.size());
            
            if (node->children[index]->keys.size() < t) {
                fill(node, index);
            }
            
            if (flag && index > node->keys.size()) {
                remove(node->children[index - 1], key);
            } else {
                remove(node->children[index], key);
            }
        }
    }
    
    void traverse(Node* node) const {
        int i;
        for (i = 0; i < node->keys.size(); i++) {
            if (!node->isLeaf) {
                traverse(node->children[i]);
            }
            std::cout << node->keys[i] << " ";
        }
        
        if (!node->isLeaf) {
            traverse(node->children[i]);
        }
    }
    
public:
    BTree(int degree) {
        t = degree;
        root = new Node(true);
    }
    
    ~BTree() {
        delete root;
    }
    
    void traverse() const {
        if (root) {
            traverse(root);
        }
        std::cout << std::endl;
    }
    
    bool search(const T& key) const {
        if (!root) {
            return false;
        }
        
        int pos;
        return search(root, key, pos);
    }
    
    void insert(const T& key) {
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
    
    void remove(const T& key) {
        if (!root) {
            return;
        }
        
        remove(root, key);
        
        if (root->keys.empty()) {
            if (root->isLeaf) {
            } else {
                Node* oldRoot = root;
                root = root->children[0];
                oldRoot->children.clear(); 
                delete oldRoot;
            }
        }
    }
};

int main() {
    BTree<int> t(3);
    
    t.insert(1);
    t.insert(3);
    t.insert(7);
    t.insert(10);
    t.insert(11);
    t.insert(13);
    t.insert(14);
    t.insert(15);
    t.insert(18);
    t.insert(16);
    t.insert(19);
    t.insert(24);
    t.insert(25);
    t.insert(26);
    
    std::cout << "Обход B-дерева: ";
    t.traverse();
    
    t.remove(13);
    std::cout << "Обход после удаления 13: ";
    t.traverse();
    
    t.remove(7);
    std::cout << "Обход после удаления 7: ";
    t.traverse();
    
    t.remove(1);
    t.remove(11);
    t.remove(14);
    t.remove(26);

    t.traverse();
    

    std::cout << "Поиск 15: " << (t.search(15) ? "найдено" : "не найдено") << std::endl;
    std::cout << "Поиск: " << (t.search(26) ? "найдено" : "не найдено") << std::endl;
    std::cout << "Поиск 13: " << (t.search(13) ? "найдено" : "не найдено") << std::endl;
    
    return 0;
}
