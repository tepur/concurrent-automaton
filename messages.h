#ifndef MESSAGES_H
#define MESSAGES_H

#include <mqueue.h>
#include <string.h>

extern const char* tester_queue_name_prefix;
extern const char* validator_input_queue_name;
extern const char* stop_word;
extern const char* empty_word;

extern const char* msg_prefix_from_run;
extern const char* msg_prefix_from_tester;
extern const char* msg_prefix_snt;

extern const char* tester_start;
extern const char* tester_end;

typedef struct {
    const char* msg;
    int prio;
} SpecialMsg;

extern SpecialMsg end_msg;
extern SpecialMsg term_msg;
extern SpecialMsg write_ended_msg;

extern int mq_send_special(mqd_t mqdes, SpecialMsg* special_msg);

#endif //MESSAGES_H
