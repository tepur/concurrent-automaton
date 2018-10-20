#ifndef STATEARRAY_H
#define STATEARRAY_H

#include <stdlib.h>

#include "mytypes.h"

//Array, which size can be set only once
typedef struct {
    state_t* data;
    int size;
} StateArray;

extern state_t get_elem(const StateArray* arr, int index);
extern void set_elem(StateArray* arr, int index, state_t elem);
extern void zero_array(StateArray* arr);
extern int init_array(StateArray* arr, state_t* states, int states_size);
extern int set_size(StateArray* arr, int size);
extern void dealloc_array(StateArray* arr);

#endif //STATEARRAY_H
