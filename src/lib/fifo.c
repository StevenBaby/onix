#include <onix/fifo.h>
#include <onix/assert.h>

static _inline u32 fifo_next(fifo_t *fifo, u32 pos)
{
    return (pos + 1) % fifo->length;
}

void fifo_init(fifo_t *fifo, char *buf, u32 length)
{
    fifo->buf = buf;
    fifo->length = length;
    fifo->head = 0;
    fifo->tail = 0;
}

bool fifo_full(fifo_t *fifo)
{
    bool full = (fifo_next(fifo, fifo->head) == fifo->tail);
    return full;
}

bool fifo_empty(fifo_t *fifo)
{
    return (fifo->head == fifo->tail);
}

char fifo_get(fifo_t *fifo)
{
    assert(!fifo_empty(fifo));
    char byte = fifo->buf[fifo->tail];
    fifo->tail = fifo_next(fifo, fifo->tail);
    return byte;
}

void fifo_put(fifo_t *fifo, char byte)
{
    while (fifo_full(fifo))
    {
        fifo_get(fifo);
    }
    fifo->buf[fifo->head] = byte;
    fifo->head = fifo_next(fifo, fifo->head);
}
