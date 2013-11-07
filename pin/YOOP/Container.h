#ifndef CONTAINER_H
#define CONTAINER_H

// This object is defined using YOOP Specification 1

// Required by YOOP1
#define THIS ((Container *)_this)

// ******************
// *   CLASS BODY   *
// ******************

typedef struct ContainerBody {
    // Members
    void * object;
    
    // Methods
    void (*delete)();
} Container;

// ******************
// * IMPLEMENTATION *
// ******************

// Removes the container and frees the memory
void defaultDeleteContainer() {
    free(THIS);
}

// Aggresive removal may remove your variables as well
// This approach may cause memory leak if the container was holding an object
// Use this mostly with primitive types
void aggressiveDeleteContainer() {
    free(THIS->object);
    free(THIS);
}

// Initializes and returns a new Container object
Container * createContainer(void * object, void (*delFunc)()) {
    Container * this = NEW(Container);
    
    this->object = object;
    this->delete = delFunc;
    
    return this;
}

// Required by YOOP1
#undef THIS

#endif
