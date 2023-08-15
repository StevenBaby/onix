#include <onix/fs.h>
#include <onix/task.h>
#include <onix/memory.h>
#include <onix/arena.h>
#include <onix/stat.h>
#include <onix/fifo.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static inode_t *pipe_open()
{
    inode_t *inode = get_free_inode();
    // 但是被占用了
    inode->dev = -FS_TYPE_PIPE;
    // 申请内存，表示缓冲队列
    inode->desc = (void *)kmalloc(sizeof(fifo_t));
    // 管道缓冲区一页内存
    inode->addr = (void *)alloc_kpage(1);
    // 两个文件
    inode->count = 2;
    // 管道类型
    inode->type = FS_TYPE_PIPE;
    // 管道操作
    inode->op = fs_get_op(FS_TYPE_PIPE);
    // 初始化输入输出设备
    fifo_init((fifo_t *)inode->desc, (char *)inode->buf, PAGE_SIZE);
    return inode;
}

static void pipe_close(inode_t *inode)
{
    if (!inode)
        return;
    inode->count--;
    if (inode->count)
        return;
    inode->type = FS_TYPE_NONE;

    // 释放描述符 fifo
    kfree(inode->desc);
    // 释放缓冲区
    free_kpage((u32)inode->addr, 1);
    // 释放 inode
    put_free_inode(inode);
}

static int pipe_read(inode_t *inode, char *data, int count, off_t offset)
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
        data[nr++] = fifo_get(fifo);
        if (inode->txwaiter)
        {
            task_unblock(inode->txwaiter, EOK);
            inode->txwaiter = NULL;
        }
    }
    return nr;
}

static int pipe_write(inode_t *inode, char *data, int count, off_t offset)
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
        fifo_put(fifo, data[nr++]);
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

    inode_t *inode = pipe_open();

    task_t *task = running_task();
    file_t *files[2];

    pipefd[0] = fd_get(&files[0]);
    if (pipefd[0] < EOK)
        goto rollback;

    pipefd[1] = fd_get(&files[1]);
    if (pipefd[1] < EOK)
        goto rollback;

    files[0]->inode = inode;
    files[0]->flags = O_RDONLY;

    files[1]->inode = inode;
    files[1]->flags = O_WRONLY;

    return EOK;

rollback:
    for (size_t i = 0; i < 2; i++)
    {
        fd_put(pipefd[i]);
    }
    return -EMFILE;
}

static fs_op_t pipe_op = {
    fs_default_nosys,
    fs_default_nosys,

    fs_default_nosys,
    pipe_close,

    fs_default_nosys,
    pipe_read,
    pipe_write,
    fs_default_nosys,

    fs_default_nosys,
    fs_default_nosys,

    fs_default_nosys,
    fs_default_nosys,
    fs_default_nosys,
    fs_default_nosys,
    fs_default_nosys,
    fs_default_nosys,
    fs_default_nosys,
};

void pipe_init()
{
    fs_register_op(FS_TYPE_PIPE, &pipe_op);
}