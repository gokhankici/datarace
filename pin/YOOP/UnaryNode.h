// This object is defined using YOOP Specification 1

#ifndef UNARYNODE_H
#define UNARYNODE_H

// Required by YOOP1
#define THIS ((UnaryNode *)___this)    

// ******************
// *   CLASS BODY   *
// ******************

typedef struct UnaryNodeBody 
{
    // Members
    struct UnaryNodeBody * prev;
    void * object;
    struct UnaryNodeBody * next;
    
    // Methods
    void (*myDelete)();
} UnaryNode;

// ******************
// * IMPLEMENTATION *
// ******************

// Initializes and returns a new UnaryNode object
UnaryNode * createUnaryNode(UnaryNode * prev, void * object, UnaryNode * next, 
                            void (*delFunc)()) 
{
    UnaryNode * __this = NEW(UnaryNode);

    __this->prev = prev;
    if(prev != NULL)
        prev->next = __this;

    __this->object = object;
    __this->next = next;
    if(next != NULL)
        next->prev = __this;

    __this->myDelete = delFunc;
    
    return __this;
}

// Removes this node and frees the memory
void defaultDeleteUnaryNode() 
{
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
