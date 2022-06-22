#ifndef ONIX_FIFO_H
#define ONIX_FIFO_H

#include <onix/types.h>

typedef struct fifo_t
{
    char *buf;
    u32 length;
    u32 head;
    u32 tail;
} fifo_t;

void fifo_init(fifo_t *fifo, char *buf, u32 length);
bool fifo_full(fifo_t *fifo);
bool fifo_empty(fifo_t *fifo);
char fifo_get(fifo_t *fifo);
void fifo_put(fifo_t *fifo, char byte);

#endif