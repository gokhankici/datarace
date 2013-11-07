// This object is defined using YOOP Specification 1

#ifndef BINARYNODE_H
#define BINARYNODE_H

// Required by YOOP1
#define THIS ((BinaryNode *)_this)

// ******************
// *   CLASS BODY   *
// ******************

typedef struct BinaryNodeBody {    
    // Members
    struct BinaryNodeBody * parent;
    void * object;
    struct BinaryNodeBody * left;
    struct BinaryNodeBody * right;
    
    // Methods
    void (*delete)();
} BinaryNode;

// ******************
// * IMPLEMENTATION *
// ******************

// Initializes and returns a new BinaryNode object
BinaryNode * createBinaryNode(BinaryNode * parent, void * object, BinaryNode * left, BinaryNode * right, void (*delFunc)()) {
    BinaryNode * this = NEW(BinaryNode);

    this->parent = parent;
    if(parent != NULL)
        parent->next = this;

    this->object = object;
    
    this->left = left;
    if(left != NULL)
        left->parent = this;

    this->right = right;
    if(right != NULL)
        right->parent = this;

    this->delete = delFunc;
    
    return this;
}

// Aggresive removal may remove your variables as well
// This approach may cause memory leak if the container was holding an object
// Use this mostly with primitive types
void aggressiveDeleteUnaryNode() {
    free(THIS->object);
    free(THIS);
}

// Removes this node and frees the memory
void defaultDeleteBinaryNode() {
    free(THIS);
}

// Required by YOOP1
#undef THIS

#endif
