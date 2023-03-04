#include <onix/types.h>
#include <onix/fs.h>
#include <onix/task.h>
#include <onix/stat.h>
#include <onix/stdio.h>
#include <onix/device.h>
#include <onix/string.h>
#include <onix/syscall.h>
#include <onix/fifo.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

int pipe_read(inode_t *inode, char *buf, int count)
{
    fifo_t *fifo = (fifo_t *)inode->desc;
    int nr = 0;
    while (nr < count)
    {
        if (fifo_empty(fifo))
        {
            assert(inode->rxwaiter == NULL);
            inode->rxwaiter = running_task();
            task_block(inode->rxwaiter, NULL, TASK_BLOCKED, TIMELESS);
        }
        buf[nr++] = fifo_get(fifo);
        if (inode->txwaiter)
        {
            task_unblock(inode->txwaiter, EOK);
            inode->txwaiter = NULL;
        }
    }
    return nr;
}

int pipe_write(inode_t *inode, char *buf, int count)
{
    fifo_t *fifo = (fifo_t *)inode->desc;
    int nr = 0;
    while (nr < count)
    {
        if (fifo_full(fifo))
        {
            assert(inode->txwaiter == NULL);
            inode->txwaiter = running_task();
            task_block(inode->txwaiter, NULL, TASK_BLOCKED, TIMELESS);
        }
        fifo_put(fifo, buf[nr++]);
        if (inode->rxwaiter)
        {
            task_unblock(inode->rxwaiter, EOK);
            inode->rxwaiter = NULL;
        }
    }
    return nr;
}

int sys_pipe(fd_t pipefd[2])
{
    // LOGK("pipe system call!!!!!!! %d\n", sizeof(fifo_t));

    inode_t *inode = get_pipe_inode();

    task_t *task = running_task();
    file_t *files[2];

    pipefd[0] = task_get_fd(task);
    files[0] = task->files[pipefd[0]] = get_file();

    pipefd[1] = task_get_fd(task);
    files[1] = task->files[pipefd[1]] = get_file();

    files[0]->inode = inode;
    files[0]->flags = O_RDONLY;

    files[1]->inode = inode;
    files[1]->flags = O_WRONLY;

    return 0;
}
