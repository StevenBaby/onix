#include <onix/fs.h>
#include <onix/assert.h>
#include <onix/task.h>
#include <onix/device.h>
#include <onix/syscall.h>
#include <onix/stat.h>

#define FILE_NR 128

file_t file_table[FILE_NR];

// 从文件表中获取一个空文件
file_t *get_file()
{
    for (size_t i = 3; i < FILE_NR; i++)
    {
        file_t *file = &file_table[i];
        if (!file->count)
        {
            file->count++;
            return file;
        }
    }
    panic("Exceed max open files!!!");
}

void put_file(file_t *file)
{
    assert(file->count > 0);
    file->count--;
    if (!file->count)
    {
        iput(file->inode);
    }
}

void file_init()
{
    for (size_t i = 3; i < FILE_NR; i++)
    {
        file_t *file = &file_table[i];
        file->mode = 0;
        file->count = 0;
        file->flags = 0;
        file->offset = 0;
        file->inode = NULL;
    }
}

fd_t sys_open(char *filename, int flags, int mode)
{
    inode_t *inode = inode_open(filename, flags, mode);
    if (!inode)
        return EOF;

    task_t *task = running_task();
    fd_t fd = task_get_fd(task);
    file_t *file = get_file();
    assert(task->files[fd] == NULL);
    task->files[fd] = file;

    file->inode = inode;
    file->flags = flags;
    file->count = 1;
    file->mode = inode->desc->mode;
    file->offset = 0;

    if (flags & O_APPEND)
    {
        file->offset = file->inode->desc->size;
    }
    return fd;
}

int sys_creat(char *filename, int mode)
{
    return sys_open(filename, O_CREAT | O_TRUNC, mode);
}

void sys_close(fd_t fd)
{
    assert(fd < TASK_FILE_NR);
    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file)
        return;

    assert(file->inode);
    put_file(file);
    task_put_fd(task, fd);
}

int sys_read(fd_t fd, char *buf, int count)
{
    task_t *task = running_task();
    file_t *file = task->files[fd];
    assert(file);
    assert(count > 0);
    int len = 0;

    if ((file->flags & O_ACCMODE) == O_WRONLY)
        return EOF;

    inode_t *inode = file->inode;
    if (inode->pipe)
    {
        len = pipe_read(inode, buf, count);
        return len;
    }
    else if (ISCHR(inode->desc->mode))
    {
        assert(inode->desc->zone[0]);
        len = device_read(inode->desc->zone[0], buf, count, 0, 0);
        return len;
    }
    else if (ISBLK(inode->desc->mode))
    {
        assert(inode->desc->zone[0]);
        device_t *device = device_get(inode->desc->zone[0]);
        assert(file->offset % BLOCK_SIZE == 0);
        assert(count % BLOCK_SIZE == 0);
        len = device_read(inode->desc->zone[0], buf, count / BLOCK_SIZE, file->offset / BLOCK_SIZE, 0);
        return len;
    }
    else
    {
        len = inode_read(inode, buf, count, file->offset);
    }
    if (len != EOF)
    {
        file->offset += len;
    }
    return len;
}

int sys_write(unsigned int fd, char *buf, int count)
{
    task_t *task = running_task();
    file_t *file = task->files[fd];
    assert(file);
    assert(count > 0);

    if ((file->flags & O_ACCMODE) == O_RDONLY)
        return EOF;

    int len = 0;
    inode_t *inode = file->inode;
    assert(inode);

    if (inode->pipe)
    {
        len = pipe_write(inode, buf, count);
        return len;
    }
    else if (ISCHR(inode->desc->mode))
    {
        assert(inode->desc->zone[0]);
        device_t *device = device_get(inode->desc->zone[0]);
        len = device_write(inode->desc->zone[0], buf, count, 0, 0);
        return len;
    }
    else if (ISBLK(inode->desc->mode))
    {
        assert(inode->desc->zone[0]);
        device_t *device = device_get(inode->desc->zone[0]);
        assert(file->offset % BLOCK_SIZE == 0);
        assert(count % BLOCK_SIZE == 0);
        len = device_write(inode->desc->zone[0], buf, count / BLOCK_SIZE, file->offset / BLOCK_SIZE, 0);
        return len;
    }
    else
    {
        len = inode_write(inode, buf, count, file->offset);
    }

    if (len != EOF)
    {
        file->offset += len;
    }

    return len;
}

int sys_lseek(fd_t fd, off_t offset, whence_t whence)
{
    assert(fd < TASK_FILE_NR);

    task_t *task = running_task();
    file_t *file = task->files[fd];

    assert(file);
    assert(file->inode);

    switch (whence)
    {
    case SEEK_SET:
        assert(offset >= 0);
        file->offset = offset;
        break;
    case SEEK_CUR:
        assert(file->offset + offset >= 0);
        file->offset += offset;
        break;
    case SEEK_END:
        assert(file->inode->desc->size + offset >= 0);
        file->offset = file->inode->desc->size + offset;
        break;
    default:
        panic("whence not defined !!!");
        break;
    }
    return file->offset;
}

int sys_readdir(fd_t fd, dirent_t *dir, u32 count)
{
    return sys_read(fd, (char *)dir, sizeof(dirent_t));
}

static int dupfd(fd_t fd, fd_t arg)
{
    task_t *task = running_task();
    if (fd >= TASK_FILE_NR || !task->files[fd])
        return EOF;

    for (; arg < TASK_FILE_NR; arg++)
    {
        if (!task->files[arg])
            break;
    }

    if (arg >= TASK_FILE_NR)
        return EOF;

    task->files[arg] = task->files[fd];
    task->files[arg]->count++;
    return arg;
}

fd_t sys_dup(fd_t oldfd)
{
    return dupfd(oldfd, 0);
}

fd_t sys_dup2(fd_t oldfd, fd_t newfd)
{
    close(newfd);
    return dupfd(oldfd, newfd);
}
