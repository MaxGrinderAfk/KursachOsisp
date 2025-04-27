#ifndef BTREE_H
#define BTREE_H

#include "libs.h"
#include "lockFreeAllocator.h"

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

#include "bTree.cpp"

#endif