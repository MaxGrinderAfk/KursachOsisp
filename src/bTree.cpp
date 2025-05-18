#ifndef BTREE_CPP
#define BTREE_CPP

#include "bTree.h"

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

// Вспомогательный метод для глубокого копирования узла
template <typename T>
typename BTree<T>::Node* BTree<T>::Node::clone() const {
    Node* newNode = new Node(isLeaf);
    
    newNode->keys = keys;
    
    for (auto child : children) {
        if (child) {
            newNode->children.push_back(child->clone());
        } else {
            newNode->children.push_back(nullptr);
        }
    }
    
    return newNode;
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

// Конструктор копирования
template <typename T>
BTree<T>::BTree(const BTree& other) : t(other.t) {
    std::shared_lock<std::shared_mutex> lock(other.tree_mutex);
    
    if (other.root) {
        root = other.root->clone();
    } else {
        root = nullptr;
    }
}

// Оператор присваивания копированием
template <typename T>
BTree<T>& BTree<T>::operator=(const BTree& other) {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lock_self(tree_mutex);
        std::shared_lock<std::shared_mutex> lock_other(other.tree_mutex);
        
        BTree temp(other);
        
        std::swap(t, temp.t);
        std::swap(root, temp.root);
    }
    return *this;
}

// Конструктор перемещения
template <typename T>
BTree<T>::BTree(BTree&& other) noexcept : t(other.t), root(other.root) {
    other.root = nullptr;
    other.t = 2;
}

// Оператор присваивания перемещением
template <typename T>
BTree<T>& BTree<T>::operator=(BTree&& other) noexcept {
    if (this != &other) {
        std::unique_lock<std::shared_mutex> lock_self(tree_mutex);
        std::unique_lock<std::shared_mutex> lock_other(other.tree_mutex);
        
        delete root;
        
        t = other.t;
        root = other.root;
        
        other.root = nullptr;
        other.t = 2;
    }
    return *this;
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

#endif