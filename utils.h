#ifndef UTILS_H
#define UTILS_H

#include <mqueue.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <errno.h>

#include "messages.h"
#include "mytypes.h"

#define PIPE_READ 0
#define PIPE_WRITE 1

extern void set_string_end(char* str, int length, int max_size);

extern mqd_t mq_open_with_msgsize(const char *name, int oflag, mode_t mode, int msgsize);

extern void cleanup_error(const char* program_name, const char* error_msg);

typedef void (*cleanupFunPtr)();

extern void raise_error(int* error_reported, mqd_t validator_desc, const char* program_name,
                        const char* function_name, const char* error_msg, cleanupFunPtr cleanup);

#endif //UTILS_H
