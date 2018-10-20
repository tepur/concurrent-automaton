#include "utils.h"

void set_string_end(char* str, int length, int max_size) {
    str[length < max_size - 1 ? length : max_size - 1] = '\0';
}

void cleanup_error(const char* program_name, const char* error_msg) {
    fprintf(stderr, "Error during cleanup in %s: %s\n", program_name, error_msg);
    fprintf(stderr," (%d; %s)\n", errno, strerror(errno));
}

void raise_error(int* error_reported, mqd_t validator_desc, const char* program_name, const char* function_name, const char* error_msg, cleanupFunPtr cleanup) {
    *error_reported = true;

    fprintf(stderr, "Error in %s: %s, function: %s", program_name, error_msg, function_name);
    fprintf(stderr," errno: (%d; %s)\n", errno, strerror(errno));
    
    if(validator_desc != (mqd_t) -1) {
        int ret = mq_send_special(validator_desc, &term_msg);
        if (ret) cleanup_error("run", "mq_send term msg");
    }

    if(cleanup != NULL) {
        cleanup();
    }
    exit(-1);
}

mqd_t mq_open_with_msgsize(const char *name, int oflag, mode_t mode, int msgsize) {
    mqd_t desc = mq_open(name, oflag, mode, NULL);
    if(desc == (mqd_t) -1) {
        return -1;
    }

    struct mq_attr attr;

    if(mq_getattr(desc, &attr)) {
        return -1;
    }

    attr.mq_msgsize = msgsize;

    if(mq_close(desc)) {
        return -1;
    }
    if(mq_unlink(name)) {
        return -1;
    }

    return mq_open(name, oflag, mode, &attr);
}
