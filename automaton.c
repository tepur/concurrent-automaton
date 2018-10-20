#include "automaton.h"

void init_automaton(Automaton* aut) {
    int i, j;
    for(i = 0; i < MAX_STATE; i++) {
        aut->is_final_state[i] = false;
    }
    for(i = 0; i < MAX_STATE; i++) {
        for(j = 0; j < MAX_LETTER; j++) {
            zero_array(&(aut->T[i][j]));
        }
    }
}
void dealloc_automaton(Automaton* a) {
    int i, j;
    for(i = 0; i < MAX_STATE; i++) {
        for(j = 0; j < MAX_LETTER; j++) {
            dealloc_array(&(a->T[i][j]));
        }
    }
}

boolean is_existential(const Automaton* a, state_t q) {
    return q >= (a->U);
}
boolean is_universal(const Automaton* a, state_t q) {
    return !(is_existential(a, q));
}
boolean is_final(const Automaton* a, state_t q) {
    return a->is_final_state[q];
}

StateArray* getT(Automaton* automaton, int state, char letter) {
    return &(automaton->T[state][letter - 'a']);
}
void print_automaton(const Automaton* a) {
    printf("Alphabet size: %d\n", a->A);
    printf("Number of states: %d\n", a->Q);
    printf("Universal states: 0,...,%d\n", (a->U - 1));
    printf("Initial state: %d\n", a->initial_state);
    printf("Final states[amount: %d]: ", a->F);
    int i; char j; int k;
    for(i = 0; i < a->Q; i++) {
        if(a->is_final_state[i]){
            printf("%d, ", i);
        }
    }
    printf("\nfunction T:\n");
    for(i = 0; i < a->Q; i++) {
        for(j = 'a'; j <= 'z'; j++) {
            if(a->T[i][j - 'a'].size > 0) {
                printf("T(%d,%c): ", i, j);
                for(k = 0; k < a->T[i][j - 'a'].size; k++) {
                    printf("%d, ", get_elem(&(a->T[i][j - 'a']), k));
                }
            }
        }
    }
}
