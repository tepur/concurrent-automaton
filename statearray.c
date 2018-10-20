#include "statearray.h"

state_t get_elem(const StateArray* arr, int index) {
    return arr->data[index];
}

void set_elem(StateArray* arr, int index, state_t elem) {
    arr->data[index] = elem;
}

void zero_array(StateArray* arr) {
    arr->size = 0;
    arr->data = NULL;
}

int set_size(StateArray* arr, int size) {
    if(size > 0) {
        arr->data = calloc(size, sizeof(state_t));
        arr->size = size;

        if(arr->data == NULL) {
            return (-1);
        }
    }
    return 0;
}

int init_array(StateArray* arr, state_t* states, int size) {
    if(set_size(arr, size) == (-1)) {
        return (-1);
    }

    int i;
    for(i = 0; i < size; i++) {
        arr->data[i] = states[i];
    }
    return 0;
}

void dealloc_array(StateArray* arr) {
    if((arr->size) > 0) {
        free(arr->data);
        arr->size = 0;
    }
}
