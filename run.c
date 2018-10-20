#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <errno.h>
#include <signal.h>

#include "mytypes.h"
#include "mylimits.h"
#include "statearray.h"
#include "automaton.h"
#include "utils.h"
#include "messages.h"

int n_of_waits = 0;
int n_of_reads = 0;
int n_of_writes = 0;

int write_pipe = (-1);
int read_pipe = (-1);

mqd_t validator_desc = (-1);

Automaton main_automaton;

void run_error(const char* function_name, const char* error_msg);

boolean error_reported = false;
void run_cleanup_error(const char* msg) {
    if(!error_reported) {
        run_error("cleanup", msg);
    }
    else { //if error has been already reported, we don't want to raise it once again - we just want to clean up what's possible and terminate
        cleanup_error("run", msg);
    }
}

void cleanup() {
    if(validator_desc != (mqd_t) -1) {
        if (mq_close(validator_desc)) run_cleanup_error("mq_close validator queue");
    }
    int i;
    for(i = 0; i < n_of_waits; i++) {
        if(wait(0) == -1) cleanup_error("run", "wait");
    }
    boolean mock_result;
    for(i = 0; i < n_of_reads; i++) {
        if((read(read_pipe, &mock_result, sizeof(mock_result))) == -1)
            cleanup_error("run", "read mock result");
    }
    mock_result = false;
    for(i = 0; i < n_of_writes; i++) {
        if(write(write_pipe, &mock_result, sizeof(mock_result)) != sizeof(mock_result))
            cleanup_error("run","write mock result");
    }

    dealloc_automaton(&main_automaton);
}

void run_error(const char* function_name, const char* error_msg) {
    raise_error(&error_reported, validator_desc, "run", function_name, error_msg, cleanup);
}

boolean accept(Automaton* automaton, const char* word, state_t cur_state, int cur_depth) {
    if(cur_depth >= (strlen(word))) {
        return is_final(automaton, cur_state);
    }

    int pipe_dsc[2];
    if (pipe(pipe_dsc) == -1) run_error("accept", "pipe for recursive calculations");

    read_pipe = pipe_dsc[PIPE_READ];
    int i;

    for(i = 1; i < getT(automaton, cur_state, word[cur_depth])->size; i++) {
        state_t q = get_elem(getT(automaton, cur_state, word[cur_depth]), i);
        switch(fork()) {
            case -1:
                run_error("accept", "fork for recursive calculations");

            case 0: //child
                write_pipe = pipe_dsc[PIPE_WRITE];

                n_of_reads = 0;
                n_of_waits = 0;
                n_of_writes = 1;

                if (close (pipe_dsc[PIPE_READ]) == -1)
                    run_error("accept", "(child) close read pipe");

                boolean run_accepted = accept(automaton, word, q, cur_depth + 1);

                if(write(pipe_dsc[PIPE_WRITE], &run_accepted, sizeof(run_accepted)) != sizeof(run_accepted))
                    run_error("accept","(child) write result");

                n_of_writes--;

                if(close (pipe_dsc[PIPE_WRITE]) == -1)
                    run_error("accept","(child) close write pipe");

                exit(0);
            default:
                n_of_waits++;
                n_of_reads++;
        }
    }
    boolean result;
    boolean all_runs_accepting = true, any_run_accepting = false;
    if(getT(automaton, cur_state, word[cur_depth])->size > 0) {
        state_t q = get_elem(getT(automaton, cur_state, word[cur_depth]), 0);
        result = accept(automaton, word, q, cur_depth + 1);
        
        if(result) {
            any_run_accepting = true;
        }
        else {
            all_runs_accepting = false;
        }
    }
    
    for(i = 1; i < getT(automaton, cur_state, word[cur_depth])->size; i++) {
        if((read(pipe_dsc[PIPE_READ], &result, sizeof(result))) == -1)
            run_error("accept", "read result");

        n_of_reads--;

        if(result) {
            any_run_accepting = true;
        }
        else {
            all_runs_accepting = false;
        }
    }

    for(i = 1; i < getT(automaton, cur_state, word[cur_depth])->size; i++) {
        if (wait(0) == -1) run_error("accept", "wait");

        n_of_waits--;
    }

    if(close (pipe_dsc[PIPE_READ]) == -1) run_error("accept", "close read pipe");

    if(is_existential(automaton, cur_state)) {
        return any_run_accepting;
    }
    else {
        return all_runs_accepting;
    }
}

int receive_data(int read_dsc, Automaton* a, int* tester_pid, char word_to_verify[]) {
    if ((read(read_dsc, &a->N, sizeof(a->N))) == -1) {
        return -1;
    }
    if ((read(read_dsc, &a->A, sizeof(a->A))) == -1) {
        return -1;
    }
    if ((read(read_dsc, &a->Q, sizeof(a->Q))) == -1) {
        return -1;
    }
    if ((read(read_dsc, &a->U, sizeof(a->U))) == -1) {
        return -1;
    }
    if ((read(read_dsc, &a->F, sizeof(a->F))) == -1) {
        return -1;
    }
    if ((read(read_dsc, &(a->initial_state), sizeof(a->initial_state))) == -1) {
        return -1;
    }

    state_t state, elem;
    int i, j;
    for (i = 0; i < a->F; i++) {
        if ((read(read_dsc, &state, sizeof(state))) == -1) {
            return -1;
        }
        a->is_final_state[state] = 1;
    }
    int letter, size;
    for (i = 3; i < (a->N); i++) {
        if ((read(read_dsc, &state, sizeof(state))) == -1) {
            return -1;
        }
        if ((read(read_dsc, &letter, sizeof(letter))) == -1) {
            return -1;
        }
        if ((read(read_dsc, &size, sizeof(size))) == -1) {
            return -1;
        }
        set_size(&(a->T[state][letter]), size);
        for (j = 0; j < size; j++) {
            if ((read(read_dsc, &elem, sizeof(elem))) == -1) {
                return -1;
            }
            set_elem(&(a->T[state][letter]), j, elem);
        }
    }

    if ((read(read_dsc, tester_pid, sizeof(*tester_pid))) == -1) {
        return -1;
    }
    if ((read(read_dsc, word_to_verify, MAX_WORD_SIZE)) == -1) {
        return -1;
    }

    return 0;
}

int send_answer(int tester_pid, boolean result, const char* word_to_verify) {
    char msg[MAX_MSG_SIZE];
	
	snprintf(msg, MAX_MSG_SIZE, "%s %d %s %d", msg_prefix_from_run, tester_pid, word_to_verify, result);
	
    int ret = mq_send(validator_desc, msg, strlen(msg), 1);
    if(ret) {
        return -1;
    }
    return 0;
}

void catch_quit_signal(int sig) {
    cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    validator_desc = mq_open(validator_input_queue_name, O_WRONLY);
    if(validator_desc == (mqd_t) -1) run_error("main", "mq_open validator queue");

    char word_to_verify[MAX_WORD_SIZE];
    int tester_pid;

    init_automaton(&main_automaton);

    struct sigaction action_quit;
    sigset_t block_mask;

    sigemptyset (&block_mask);

    action_quit.sa_handler = catch_quit_signal;
    action_quit.sa_mask = block_mask;
    action_quit.sa_flags = 0;

    if (sigaction (RUN_QUIT_SIGNAL, &action_quit, 0) == -1)
        run_error("main", "sigaction RUN_QUIT_SIGNAL");

    if(receive_data(atoi(argv[1]), &main_automaton, &tester_pid, word_to_verify)) {
        run_error("main", "receive_data");
    }
    
    if(strcmp(word_to_verify, empty_word) == 0) {
        word_to_verify[0] = '\0';
    }
    boolean result = accept(&main_automaton, word_to_verify, main_automaton.initial_state, 0);

    if(word_to_verify[0] == '\0') {
        word_to_verify[0] = empty_word[0];
    }    
    
    if(send_answer(tester_pid, result, word_to_verify)) {
        run_error("main", "send_answer");
    }

    cleanup();
    return 0;
}

