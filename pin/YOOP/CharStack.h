#ifndef CHARSTACK_H
#define CHARSTACK_H

// This object is defined using YOOP Specification 1

// Required by YOOP1
#define THIS ((CharStack *)_this)

// ******************
// *   CLASS BODY   *
// ******************

typedef struct CharStackBody {
    // Members
    char * array;
    unsigned int size;
    unsigned int currentCapacity;
    
    // Methods
    void (*push)(char ch);
    char (*pop)();
    char (*peek)();
    void (*increaseCapacity)(unsigned int);
    void (*delete)();
} CharStack;

// ******************
// * IMPLEMENTATION *
// ******************

void pushCharStack(char ch) {
    if (THIS->size == THIS->currentCapacity)
        THIS->increaseCapacity(THIS->currentCapacity);
        
    THIS->array[THIS->size] = ch;
    THIS->size++;
    THIS->array[THIS->size] = '\0';
}

char popCharStack() {
    if (!THIS->size)
        return '\0';

    THIS->size--;
    char current = THIS->array[THIS->size];
    THIS->array[THIS->size] = '\0';
    
    return current;
}

char peekCharStack() {
    return THIS->size ? THIS->array[THIS->size-1] : '\0';
}

void increaseCharStackCapacity(unsigned int by) {
    THIS->currentCapacity += by;
    char * newArray = NEW_ARRAY(char, THIS->currentCapacity + 1);
    strcpy(newArray, THIS->array);
    free(THIS->array);
    THIS->array = newArray;
}

void defaultDeleteCharStack() {
    free(THIS->array);
    free(THIS);
}

CharStack * createCharStack(unsigned int initialCapacity) {
    CharStack * this = NEW(CharStack);
    
    this->array = NEW_ARRAY(char, initialCapacity + 1);
    this->size = 0;
    this->currentCapacity = initialCapacity;
    
    this->push = pushCharStack;
    this->pop = popCharStack;
    this->peek = peekCharStack;
    this->increaseCapacity = increaseCharStackCapacity;
    this->delete = defaultDeleteCharStack;

    this->array[0] = '\0';
    
    return this;
}

// Required by YOOP1
#undef THIS

#endif
