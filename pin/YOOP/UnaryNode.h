// This object is defined using YOOP Specification 1

#ifndef UNARYNODE_H
#define UNARYNODE_H

// Required by YOOP1
#define THIS ((UnaryNode *)_this)    

// ******************
// *   CLASS BODY   *
// ******************

typedef struct UnaryNodeBody {
    // Members
    struct UnaryNodeBody * prev;
    void * object;
    struct UnaryNodeBody * next;
    
    // Methods
    void (*delete)();
} UnaryNode;

// ******************
// * IMPLEMENTATION *
// ******************

// Initializes and returns a new UnaryNode object
UnaryNode * createUnaryNode(UnaryNode * prev, void * object, UnaryNode * next, 
                            void (*delFunc)()) {
    UnaryNode * this = NEW(UnaryNode);

    this->prev = prev;
    if(prev != NULL)
        prev->next = this;

    this->object = object;
    this->next = next;
    if(next != NULL)
        next->prev = this;

    this->delete = delFunc;
    
    return this;
}

// Removes this node and frees the memory
void defaultDeleteUnaryNode() {
    free(THIS);
}

// Aggresive removal may remove your variables as well
// This approach may cause memory leak if the container was holding an object
// Use this mostly with primitive types
void aggressiveDeleteUnaryNode() {
    free(THIS->object);
    free(THIS);
}

// Required by YOOP1
#undef THIS

#endif
