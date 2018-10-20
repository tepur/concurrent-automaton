#include "messages.h"

const char* validator_input_queue_name = "/vInput";
const char* tester_queue_name_prefix = "/tester";

const char* msg_prefix_from_run = "RUN";
const char* msg_prefix_from_tester = "TESTER";
const char* msg_prefix_snt = "SNT";

const char* tester_start = "TESTER_START!";
const char* tester_end = "TESTER_END!";

const char* stop_word = "!";
const char* empty_word = "EMPTY!";

SpecialMsg end_msg = {"END!", 2};
SpecialMsg term_msg = {"TERM!", 3};
SpecialMsg write_ended_msg = {"WRITE_ENDED!", 2};

int mq_send_special(mqd_t mqdes, SpecialMsg* special_msg) {
    return mq_send(mqdes, special_msg->msg, strlen(special_msg->msg), special_msg->prio);
}
