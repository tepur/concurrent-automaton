#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <signal.h>

#include "mytypes.h"
#include "mylimits.h"
#include "messages.h"
#include "stats.h"
#include "utils.h"

#define END_READ_SIGNAL SIGQUIT

mqd_t validator_desc = -1;
int child_pid = -1;

char my_answers_queue_name[MAX_QUEUE_NAME_SIZE];
mqd_t my_answers_desc = -1;

void tester_receiver_error(const char* function_name, const char* error_msg);

void print_stats(Stats* stats) {
    printf("Snt: %d\nRcd: %d\nAcc: %d\n", stats->snt, stats->rcd, stats->acc);
}

boolean error_reported = false;
void tester_receiver_cleanup_error(const char* msg) {
    if(!error_reported) {
        tester_receiver_error("cleanup", msg);
    }
    else { //if error has been already reported, we don't want to raise it once again - we just want to clean up what's possible and terminate
        cleanup_error("tester (receiver)", msg);
    }
}

void receiver_cleanup() {
    if(my_answers_desc != (mqd_t) -1) {
        if (mq_close(my_answers_desc)) tester_receiver_cleanup_error("mq_close");
        if (mq_unlink(my_answers_queue_name)) tester_receiver_cleanup_error("mq_unlink");
    }
    if(validator_desc != (mqd_t) -1) {
        if (mq_close(validator_desc)) tester_receiver_cleanup_error("mq_close");
    }
    if(child_pid != (-1)) {
        if(wait(0) == -1) tester_receiver_cleanup_error("wait");
    }
}

void tester_sender_error(const char* function_name, const char* error_msg) {
    raise_error(&error_reported, validator_desc, "tester (sender)", function_name, error_msg, NULL);
}

void tester_receiver_error(const char* function_name, const char* error_msg) {
    if(child_pid != (-1)) {
        kill(child_pid, END_READ_SIGNAL);
    }
    raise_error(&error_reported, validator_desc, "tester (receiver)", function_name, error_msg, receiver_cleanup);
}

int snt;

int send_snt_msg() {
    static char snt_msg[MAX_MSG_SIZE];

    snprintf(snt_msg, MAX_MSG_SIZE, "%s %d", msg_prefix_snt , snt);

    int ret = mq_send(my_answers_desc, snt_msg, strlen(snt_msg), 1);
    if(ret) {
        return -1;
    }
    return 0;
}

void catch_end_read_signal(int sig) {
    send_snt_msg();
    exit(0);
}

int send_request(int main_pid, const char* word, int prio) {
    static char request_msg[MAX_MSG_SIZE];

    snprintf(request_msg, MAX_MSG_SIZE, "%s %d %s", msg_prefix_from_tester ,main_pid, word);
    
    int ret = mq_send(validator_desc, request_msg, strlen(request_msg), prio);
    if (ret) {
        return -1;
    }
    return 0;
}

void read_input_and_send_requests(int main_pid) {
    char word[MAX_WORD_SIZE];

    if(send_request(main_pid, tester_start, 2))
        tester_sender_error("read_input_and_send_requests", "send_request start");

    while(fgets(word, sizeof(word), stdin) != NULL){
        if(strcmp(word, "!\n") != 0) {
            snt++;
        }

        if(strcmp(word, "\n") == 0) {
            strcpy(word, empty_word);
        }        
        
        if(send_request(main_pid, word, 1))
            tester_sender_error("read_input_and_send_requests", "send_request word");
    }

    if(send_request(main_pid, tester_end, 2))
        tester_sender_error("read_input_and_send_requests", "send_request end");

    if(send_snt_msg())
        tester_sender_error("read_input_and_send_requests", "send_snt_msg");
}

void print_word_result(char* word, boolean accepted) {
    if(strcmp(word, empty_word) == 0) {
        printf(" ");
    }
    else{
        printf("%s ", word);
    }
    if(accepted) {
        printf("A\n");
    }
    else {
        printf("N\n");
    }
}

void receive_answers_and_print_them(Stats* stats) {
    char answer_msg[MAX_MSG_SIZE];
    char word[MAX_WORD_SIZE];

    boolean accepted;

    int ret;
    int number, queries_sent = (-1);
    boolean snt_received = false;

    while((queries_sent > (stats->rcd)) || (!snt_received)) {
        ret = mq_receive(my_answers_desc, answer_msg, MAX_MSG_SIZE, NULL);
        if(ret < 0) tester_receiver_error("receive_answers_and_print_them", "mq_receive answer");

        set_string_end(answer_msg, ret, MAX_MSG_SIZE);
        
        sscanf(answer_msg, "%s %d", word, &number);

        if(strcmp(word, end_msg.msg) == 0) {
            queries_sent = number;
            if(kill(child_pid, END_READ_SIGNAL))
                tester_receiver_error("receive_answers_and_print_them", "kill sender");
        }
        else if(strcmp(word, term_msg.msg) == 0) {
            queries_sent = (-2);
            if(kill(child_pid, END_READ_SIGNAL))
                tester_receiver_error("receive_answers_and_print_them", "mq_receive answer");
        }
        else if(strcmp(word, msg_prefix_snt) == 0) {
            snt_received = true;
            stats->snt = number;
            if(queries_sent == (-1)) {
                queries_sent = number;
            }
        }
        else { //answer from validator
            accepted = number;

            stats->rcd++;
            if (accepted) {
                stats->acc++;
            }

            print_word_result(word, accepted);
        }
    }
}

void print_summary(Stats* stats) {
    print_stats(stats);
}

int main() {
    Stats my_stats;
    init_stats(&my_stats);

    int pid = getpid();

    snprintf(my_answers_queue_name, MAX_QUEUE_NAME_SIZE, "%s%d", tester_queue_name_prefix, pid);

    my_answers_desc = mq_open_with_msgsize(my_answers_queue_name, O_RDWR | O_CREAT, 0777, MAX_MSG_SIZE);
    if(my_answers_desc == (mqd_t) -1)
        tester_receiver_error("main", "mq_open_with_maxsize my queue");

    validator_desc = mq_open(validator_input_queue_name, O_WRONLY);
    if(validator_desc == (mqd_t) -1)
        tester_receiver_error("main", "mq_open validator queue");

    struct sigaction action_end_read;
    sigset_t block_mask;

    sigemptyset (&block_mask);

    switch(child_pid = fork()) {
        case -1:
            tester_receiver_error("main", "fork");

        case 0: //child - read input and send requests

            //set end_read_signal handler
            action_end_read.sa_handler = catch_end_read_signal;
            action_end_read.sa_mask = block_mask;
            action_end_read.sa_flags = 0;

            if (sigaction (END_READ_SIGNAL, &action_end_read, 0) == -1)
                tester_sender_error("main", "sigaction END_READ_SIGNAL");

            read_input_and_send_requests(pid);
            exit(0);

        default: //parent - receive answers and print them
            printf("PID: %d\n", pid);
            receive_answers_and_print_them(&my_stats);
            print_summary(&my_stats);
            receiver_cleanup();
    }

    return 0;
}
