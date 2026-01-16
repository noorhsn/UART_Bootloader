#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef enum {
    PROTO_OK = 0,
    PROTO_ERR = -1,
} proto_status_t;

int protocol_run(void);

#endif // PROTOCOL_H
