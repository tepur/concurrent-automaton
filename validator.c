#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <errno.h>

#include "mytypes.h"
#include "statearray.h"
#include "automaton.h"
#include "stats.h"
#include "messages.h"
#include "utils.h"

#define MAX_LINE_SIZE 1001
#define MAX_PREFIX_SIZE 300

mqd_t input_desc = (-1);

int max_pid;
mqd_t* tester_desc = NULL;
boolean* queue_to_tester_active = NULL;
Stats* tester_stats = NULL;
boolean* tester_sent_end = NULL;

Stats validator_stats;
int n_of_writers_waits = 0;
int n_of_runners_waits = 0;

Automaton main_automaton;

void print_stats(Stats* stats) {
    printf("Rcd: %d\nSnt: %d\nAcc: %d\n", stats->rcd, stats->snt, stats->acc);
}
void print_tester_stats(int tester_pid, Stats* tester_stats) {
    printf("PID: %d\nRcd: %d\nAcc: %d\n", tester_pid, tester_stats->rcd, tester_stats->acc);
}

int read_automaton_from_stdin(Automaton* aut) {
    char line[MAX_LINE_SIZE];
    char *line_ptr;

    if(fgets(line, sizeof(line), stdin) == NULL) {
        return (-1);
    }
    line_ptr = line;
    aut->N = strtol(line_ptr, &line_ptr, 10);
    aut->A = strtol(line_ptr, &line_ptr, 10);
    aut->Q = strtol(line_ptr, &line_ptr, 10);
    aut->U = strtol(line_ptr, &line_ptr, 10);
    aut->F = strtol(line_ptr, &line_ptr, 10);

    if(fgets(line, sizeof(line), stdin) == NULL) {
        return (-1);
    }
    line_ptr = line;
    aut->initial_state = strtol(line_ptr, &line_ptr, 10);

    int final_state, i;
    if(fgets(line, sizeof(line), stdin) == NULL) {
        return (-1);
    }
    line_ptr = line;
    for(i = 0; i < aut->F; i++) {
        final_state = strtol(line_ptr, &line_ptr, 10);
        aut->is_final_state[final_state] = true;
    }

    state_t q;

    state_t p[MAX_STATE];
    int p_size = 0;
    char a;
    for(i = 0; i < (aut->N - 3); i++) {
        if(fgets(line, sizeof(line), stdin) == NULL) {
            return (-1);
        }

        line_ptr = line;
        q = strtol(line_ptr, &line_ptr, 10);

        line_ptr++;
        a = (*line_ptr);
        line_ptr++;

        p_size = 0;
        while(*line_ptr != '\n') {
            p[p_size] = strtol(line_ptr, &line_ptr, 10);
            p_size++;
        }

        if(init_array(getT(aut, q, a), p, p_size) == (-1)) {
            return (-1);
        }
    }
    return 0;
}
void print_summary() {
    print_stats(&validator_stats);
    for(int i = 0; i < max_pid; i++) {
        if(tester_stats[i].rcd > 0) {
            print_tester_stats(i, &tester_stats[i]);
        }
    }
}

int write_data(int pipe_write_dsc, Automaton* a, int tester_pid, char* word) {
    if (write(pipe_write_dsc, &(a->N), sizeof(a->N)) != sizeof(a->N)) {
        return -1;
    }
    if (write(pipe_write_dsc, &(a->A), sizeof(a->A)) != sizeof(a->A)) {
        return -1;
    }
    if (write(pipe_write_dsc, &(a->Q), sizeof(a->Q)) != sizeof(a->Q)) {
        return -1;
    }
    if (write(pipe_write_dsc, &(a->U), sizeof(a->U)) != sizeof(a->U)) {
        return -1;
    }
    if (write(pipe_write_dsc, &(a->F), sizeof(a->F)) != sizeof(a->F)) {
        return -1;
    }
    if (write(pipe_write_dsc, &(a->initial_state), sizeof(a->initial_state)) != sizeof(a->initial_state)) {
        return -1;
    }

    int i, j, k;
    for(i = 0; i < MAX_STATE; i++) {
        if(a->is_final_state[i]) {
            if (write(pipe_write_dsc, &i, sizeof(i)) != sizeof(i)) {
                return -1;
            }
        }
    }
    state_t elem;
    for(i = 0; i < MAX_STATE; i++) {
        for(j = 0; j < MAX_LETTER; j++) {
            if(a->T[i][j].size > 0) {
                if(write(pipe_write_dsc, &i, sizeof(i)) != sizeof(i)) {
                    return -1;
                }
                if(write(pipe_write_dsc, &j, sizeof(j)) != sizeof(j)) {
                    return -1;
                }
                if(write(pipe_write_dsc, &a->T[i][j].size, sizeof(a->T[i][j].size)) != sizeof(a->T[i][j].size)) {
                    return -1;
                }
                for(k = 0; k < a->T[i][j].size; k++) {
                    elem = get_elem(&(a->T[i][j]), k);
                    if(write(pipe_write_dsc, &elem, sizeof(elem)) != sizeof(elem)) {
                        return -1;
                    }
                }
            }
        }
    }

    if (write(pipe_write_dsc, &tester_pid, sizeof(tester_pid)) != sizeof(tester_pid)) {
        return -1;
    }
    if (write(pipe_write_dsc, word, MAX_WORD_SIZE) != MAX_WORD_SIZE) {
        return -1;
    }

    return 0;
}

int open_tester_queue(int tester_pid) {
    static char tester_queue_name[MAX_QUEUE_NAME_SIZE];

    if(!queue_to_tester_active[tester_pid]){
        snprintf(tester_queue_name, MAX_QUEUE_NAME_SIZE, "%s%d", tester_queue_name_prefix, tester_pid);
        
        tester_desc[tester_pid] = mq_open(tester_queue_name, O_WRONLY);
        if(tester_desc[tester_pid] == (mqd_t) -1) {
            return -1;
        }

        queue_to_tester_active[tester_pid] = true;
    }
    return 0;
}

int try_to_close_tester_queue(int tester_pid) {
    static char tester_queue_name[MAX_QUEUE_NAME_SIZE];

    if(queue_to_tester_active[tester_pid]){
        if(tester_sent_end[tester_pid] && tester_stats[tester_pid].rcd == tester_stats[tester_pid].snt) {
            snprintf(tester_queue_name, MAX_QUEUE_NAME_SIZE, "%s%d", tester_queue_name_prefix, tester_pid);

            if(mq_close(tester_desc[tester_pid])) {
                return -1;
            }

            queue_to_tester_active[tester_pid] = false;
        }
    }
    return 0;
}

int send_special_msg(int tester_pid, SpecialMsg *special_msg) { //for sending END! and TERM! messages
    static char tester_send_msg[MAX_MSG_SIZE];
    int number = (-1);

    if(strcmp(special_msg->msg, end_msg.msg) == 0) {
        number = tester_stats[tester_pid].rcd;
    }

    snprintf(tester_send_msg, MAX_MSG_SIZE, "%s %d", special_msg->msg, number);
    
    int ret = mq_send(tester_desc[tester_pid], tester_send_msg, strlen(tester_send_msg), special_msg->prio);
    if(ret) {
        return -1;
    }
    return 0;
}

int send_special_msg_to_all_testers(SpecialMsg *special_msg) {
    int i;
    for(i = 0; i < max_pid; i++) {
        if(queue_to_tester_active[i]) {
            if(send_special_msg(i, special_msg)) {
                return -1;
            }
        }
    }
    return 0;
}

int read_max_pid() {
    FILE* fp;
    char max_pid_buff[MAX_NUMBER_STR_SIZE];

    fp = fopen("/proc/sys/kernel/pid_max", "r");
    if(fp == NULL) {
        return -1;
    }

    if(fgets(max_pid_buff, 1000, fp) == NULL) {
        return -1;
    }
    max_pid = strtol(max_pid_buff, NULL, 10);

    if(fclose(fp)) {
        return -1;
    }

    return 0;
}
int init_max_pid_data() {
    if((queue_to_tester_active = calloc(max_pid, sizeof(boolean))) == NULL) {
        return -1;
    }
    if((tester_desc = calloc(max_pid, sizeof(mqd_t))) == NULL) {
        return -1;
    }
    if((tester_stats = calloc(max_pid, sizeof(Stats))) == NULL) {
        return -1;
    }
    if((tester_sent_end = calloc(max_pid, sizeof(boolean))) == NULL) {
        return -1;
    }

    return 0;
}
void free_max_pid_data() {
    if(queue_to_tester_active != NULL)
        free(queue_to_tester_active);
    if(tester_desc != NULL)
        free(tester_desc);
    if(tester_stats != NULL)
        free(tester_stats);
    if(tester_sent_end != NULL)
        free(tester_sent_end);
}

void validator_error(const char* function_name, const char* error_msg);

boolean error_reported = false;
void validator_cleanup_error(const char* msg) {
    if(!error_reported) {
        validator_error("cleanup", msg);
    }
    else { //if error has been already reported, we don't want to raise it once again - we just want to clean up what's possible and terminate
        cleanup_error("validator", msg);
    }
}

void child_cleanup() {
    if(input_desc != (mqd_t) -1) {
        if (mq_close(input_desc)) validator_cleanup_error("mq_close");
    }
}
void cleanup() { //only clears data, doesn't terminate other programs
    int i;
    for(i = 0; i < max_pid; i++) {
        if(queue_to_tester_active[i]) {
            if(mq_close(tester_desc[i])) validator_cleanup_error("mq_close");
        }
    }

    for(i = 0; i < n_of_runners_waits; i++) {
        if(wait(0) == -1) validator_cleanup_error("wait");
    }
    for(i = 0; i < n_of_writers_waits; i++) {
        if(wait(0) == -1) validator_cleanup_error("wait");
    }

    free_max_pid_data();
    dealloc_automaton(&main_automaton);
    
    if(input_desc != (mqd_t) -1) {
        if (mq_close(input_desc)) validator_cleanup_error("mq_close");
        if (mq_unlink(validator_input_queue_name)) validator_cleanup_error("mq_unlink");
    }
}

void validator_error(const char* function_name, const char* error_msg) {
    send_special_msg_to_all_testers(&term_msg);
    raise_error(&error_reported, -1, "validator", function_name, error_msg, cleanup);
}

void validator_child_error(const char* function_name, const char* error_msg) {
    raise_error(&error_reported, input_desc, "validator (child)", function_name, error_msg, child_cleanup);
}

int main() {
    input_desc = mq_open_with_msgsize(validator_input_queue_name, O_RDWR | O_CREAT, 0777, MAX_MSG_SIZE);
    if(input_desc == (mqd_t) -1) validator_error("main", "mq_open_with_msgsize validator queue");

    if(read_max_pid()) validator_error("main", "read_max_pid");
    if(init_max_pid_data()) validator_error("main", "init_max_pid_data");

    init_stats(&validator_stats);

    init_automaton(&main_automaton);
    if(read_automaton_from_stdin(&main_automaton)) validator_error("main", "read_automaton_from_stdin");

    char received_msg[MAX_MSG_SIZE];
    char tester_send_msg[MAX_MSG_SIZE];

    char prefix[MAX_PREFIX_SIZE];
    int tester_pid;
    char word[MAX_WORD_SIZE];
    boolean accepted;

    boolean validator_stopped = false;
    int ret;

    while((!validator_stopped) || (validator_stats.rcd > validator_stats.snt)) {
        ret = mq_receive(input_desc, received_msg, MAX_MSG_SIZE, NULL);
        if(ret < 0) validator_error("main", "mq_receive message from input queue");

        set_string_end(received_msg, ret, MAX_MSG_SIZE);
        
        sscanf(received_msg, "%s", prefix);

        if(strcmp(prefix, write_ended_msg.msg) == 0) {
            if(wait(0) == -1) validator_error("main", "wait");
            n_of_writers_waits--;
        }
        else if(strcmp(prefix, term_msg.msg) == 0) { //term msg from anyone
            validator_stopped = true;
            if(send_special_msg_to_all_testers(&term_msg)) validator_error("main", "wait");
            break;
        }
        else if(strcmp(prefix, msg_prefix_from_run) == 0) { //message from run
            sscanf(received_msg, "%s %d %s %d", prefix, &tester_pid, word, &accepted);

            if(accepted) {
                validator_stats.acc++;
                tester_stats[tester_pid].acc++;
            }
            snprintf(tester_send_msg, MAX_MSG_SIZE, "%s %d", word, accepted);
            
            ret = mq_send(tester_desc[tester_pid], tester_send_msg, strlen(tester_send_msg), 1);
            if(ret) validator_error("main", "mq_send message to tester");
            
            tester_stats[tester_pid].snt++;     
            if(try_to_close_tester_queue(tester_pid)) validator_error ("main","close_tester_queue");                                                                                   
            validator_stats.snt++;
            
            if(wait(0) == -1) validator_error("main", "wait");
            n_of_runners_waits--;
        }
        else{ //message from tester
            sscanf(received_msg, "%s %d %s", prefix, &tester_pid, word);

            if(!validator_stopped) {
                if(strcmp(word, stop_word) == 0) {
                    validator_stopped = true;
                    if(send_special_msg_to_all_testers(&end_msg)) validator_error("main", "send_special_msg_to_all_testers");
                }
                else if(strcmp(word, tester_start) == 0) {
                    if(open_tester_queue(tester_pid)) validator_error("main", "open_tester_queue");
                }
                else if(strcmp(word, tester_end) == 0) {
                    tester_sent_end[tester_pid] = true;
                    if(try_to_close_tester_queue(tester_pid)) validator_error("main", "close_tester_queue");
                }
                else {
                    int pipe_dsc[2];
                    int runner_pid;

                    if (pipe(pipe_dsc) == -1) validator_error("main", "pipe");

                    switch (runner_pid = fork()) {
                        case -1:
                            validator_error("main", "fork");
                        case 0:
                            if (close (pipe_dsc [PIPE_WRITE]) == -1)
                                validator_child_error("main", "(runner child) close write pipe");

                            char pipe_read_dsc_str[10];
                            sprintf(pipe_read_dsc_str, "%d", pipe_dsc[0]);

                            if(execl("run", "run", pipe_read_dsc_str, NULL) == -1)
                                validator_child_error("main", "(runner child) execl run");

                            exit(1);

                        default:
                            n_of_runners_waits++;
                            
                            if (close (pipe_dsc [PIPE_READ]) == -1)
                                validator_error("main", "(parent) close read pipe");
                    }

                    switch (fork()) {
                        case -1:
                            validator_error("main", "fork");
                        case 0:
                            if(write_data(pipe_dsc[PIPE_WRITE], &main_automaton, tester_pid, word)) {
                                kill(runner_pid, RUN_QUIT_SIGNAL);
                                validator_child_error("main", "(writer child) write_data");
                            }
                            if (close (pipe_dsc [PIPE_WRITE]) == -1)
                                validator_child_error("main", "(writer child) close write pipe");

                                ret = mq_send_special(input_desc, &write_ended_msg);
                                if(ret) validator_child_error("main", "(writer child) mq_send write ended msg");

                            exit(0);
                        default:
                            n_of_writers_waits++;

                            if (close (pipe_dsc [PIPE_WRITE]) == -1)
                                validator_error("main", "(writer child) close write pipe");
                    }

                    validator_stats.rcd++;
                    tester_stats[tester_pid].rcd++;
                }
            }
            else if(strcmp(word, tester_start) == 0) {
                if(open_tester_queue(tester_pid)) validator_error("main", "open_tester_queue");
                send_special_msg(tester_pid, &end_msg);
            }
        }
    }
    print_summary();

    cleanup();

    return 0;
}
