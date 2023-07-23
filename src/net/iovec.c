#include <onix/net/socket.h>
#include <onix/arena.h>
#include <onix/task.h>
#include <onix/string.h>
#include <onix/memory.h>

err_t iovec_check(iovec_t *iov, int iovlen, int write)
{
    if (!iov)
        return -EFAULT;
    if (iovlen < 0)
        return -EINVAL;

    bool user = true;
    if (running_task()->uid == KERNEL_USER)
        user = false;

    if (!memory_access(iov, iovlen * sizeof(iovec_t), false, user))
        return -EFAULT;

    for (; iovlen > 0; iov++, iovlen--)
    {
        if (iov->size <= 0)
            return -EINVAL;
        if (!iov->base)
            return -EINVAL;
        if (!memory_access(iov->base, iov->size, write, user))
            return -EFAULT;
    }
    return EOK;
}

size_t iovec_size(iovec_t *iov, int iovlen)
{
    size_t size = 0;

    while (iovlen > 0)
    {
        size += iov->size;
        iov++;
        iovlen--;
    }
    return size;
}

iovec_t *iovec_dup(iovec_t *iov, int iovlen)
{
    iovec_t *newiov;

    newiov = (iovec_t *)kmalloc(iovlen * sizeof(iovec_t));
    if (!newiov)
        return NULL;

    memcpy(newiov, iov, iovlen * sizeof(iovec_t));
    return newiov;
}

int iovec_read(iovec_t *iov, int iovlen, char *buf, size_t count)
{
    size_t read = 0;
    ;

    for (; count > 0 && iovlen > 0; iov++, iovlen--)
    {
        if (iov->size <= 0)
            continue;

        size_t len = iov->size;
        if (count < iov->size)
            len = count;

        memcpy(buf, iov->base, len);

        read += len;
        iov->base += len;
        iov->size -= len;

        buf += len;
        count -= len;
    }
    return read;
}

int iovec_write(iovec_t *iov, int iovlen, char *buf, size_t count)
{
    size_t written = 0;

    for (; count > 0 && iovlen > 0; iov++, iovlen--)
    {
        if (iov->size <= 0)
            continue;
        size_t len = iov->size;
        if (count < iov->size)
            len = count;

        memcpy(iov->base, buf, len);
        written += len;

        iov->base += len;
        iov->size -= len;

        buf += len;
        count -= len;
    }

    return written;
}