#ifndef AUTOMATON_H
#define AUTOMATON_H

#include <stdio.h>

#include "mylimits.h"
#include "mytypes.h"
#include "statearray.h"

typedef struct {
    int N, A, Q, U, F;
    state_t initial_state;
    boolean is_final_state[MAX_STATE];
    StateArray T[MAX_STATE][MAX_LETTER];
} Automaton;

extern StateArray* getT(Automaton* automaton, int state, char letter);
extern void print_automaton(const Automaton* a);
extern void init_automaton(Automaton* aut);
extern boolean is_existential(const Automaton* a, state_t q);
extern boolean is_universal(const Automaton* a, state_t q);
extern boolean is_final(const Automaton* a, state_t q);
extern void dealloc_automaton(Automaton* a);

#endif //AUTOMATON_H
