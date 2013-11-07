#ifndef ARRAYLIST_H
#define ARRAYLIST_H

// This object is defined using YOOP Specification 1

#include "Container.h"

// Required by YOOP1
#define THIS ((ArrayList *)_this)

// ******************
// *   CLASS BODY   *
// ******************

typedef struct ArrayListBody {
    // Members
    Container ** array;
    unsigned int size;
    unsigned int currentCapacity;

    // Methods
    void * (*get)(unsigned int);
    void (*remove)(unsigned int);
    void (*increaseCapacity)(unsigned int);
    void (*add)(void *, void (*delFunc)());
    void (*push)(void *);
    void * (*pop)();
    void (*set)(unsigned int, void *, void (*delFunc)());
    void (*insert)(unsigned int, void *, void (*delFunc)());
    void (*delete)();
} ArrayList;

// ******************
// * IMPLEMENTATION *
// ******************

void * getArrayListObject(unsigned int index) {
    if (0 <= index && index < THIS->size)
    {
        return THIS->array[index]->object;
    }
    else
    {
        return NULL;
    }
}

void removeArrayListObject(unsigned int index) {
    if (0 <= index && index < THIS->size)
    {
        Container * removed = THIS->array[index];

        // Shift elements
        int i;
        for (i = index + 1; i < THIS->size; i++)
        {
            THIS->array[i-1] = THIS->array[i];
        }

        THIS->size--;
        OOP_CALL(removed, delete());
    }
    else
    {
        // DO NOTHING
    }
}

void increaseArrayListCapacity(unsigned int by) {
    THIS->currentCapacity += by;
    Container ** newArray = NEW_ARRAY(Container *, THIS->currentCapacity);
    memcpy(newArray, THIS->array, THIS->size * sizeof(Container *));
    free(THIS->array);
    THIS->array = newArray;
}

void addArrayListObject(void * object, void (*delFunc)()) {
    if (THIS->size >= THIS->currentCapacity)
    {
        THIS->increaseCapacity(THIS->currentCapacity < 100000
                                ? THIS->currentCapacity : 100000);
    }

    THIS->array[THIS->size] = createContainer(object, delFunc);
    THIS->size++;
}

void pushArrayListObject(void * object) {
    if (THIS->size >= THIS->currentCapacity)
    {
        THIS->increaseCapacity(THIS->currentCapacity < 100000
                                ? THIS->currentCapacity : 100000);
    }

    THIS->array[THIS->size] = createContainer(object, defaultDeleteContainer);
    THIS->size++;
}

void * popArrayListObject() {
    if (THIS->size == 0)
    {
        return NULL;
    }
    else
    {
        void * object = THIS->get(THIS->size - 1);
        THIS->remove(THIS->size - 1);
        return object;
    }
}

void setArrayListObject(unsigned int index, void * object, void (*delFunc)()) {
    ArrayList * this = THIS;

    if (0 <= index && index < this->size)
    {
        OOP_CALL(this->array[index], delete());
        this->array[index] = createContainer(object, delFunc);
    }
    else
    {
        this->add(object, delFunc);
    }
}

void insertArrayListObject(unsigned int index, void * object, void (*delFunc)()) {
    if (0 <= index && index < THIS->size)
    {
        if (THIS->size == THIS->currentCapacity)
        {
            THIS->increaseCapacity(THIS->currentCapacity < 100000
                                    ? THIS->currentCapacity : 100000);
        }

        THIS->size++;
        //memmove(THIS->array+index+1, THIS->array+index, THIS->size-index);
        //THIS->array[index] = createContainer(object, delFunc);

        // Shift the elements
        Container * con1 = createContainer(object, delFunc);
        Container * con2;
        int i;
        for (i = index; i < THIS->size; i++)
        {
            // SWAP
            con2 = THIS->array[i];
            THIS->array[i] = con1;
            con1 = con2;
        }
    }
    else
    {
        THIS->add(object, delFunc);
    }
}

void defaultDeleteArrayList() {
    ArrayList * this = THIS;

    int i;
    for (i = 0; i < this->size; i++)
        OOP_CALL(this->array[i], delete());

    free(this);
}

ArrayList * createArrayList(unsigned int initialCapacity) {
    ArrayList * this = NEW(ArrayList);

    this->array = NEW_ARRAY(Container *, initialCapacity);
    this->size = 0;
    this->currentCapacity = initialCapacity;

    this->get = getArrayListObject;
    this->remove = removeArrayListObject;
    this->increaseCapacity = increaseArrayListCapacity;
    this->add = addArrayListObject;
    this->push = pushArrayListObject;
    this->pop = popArrayListObject;
    this->set = setArrayListObject;
    this->insert = insertArrayListObject;
    this->delete = defaultDeleteArrayList;

    return this;
}

// Required by YOOP1
#undef THIS

#endif
