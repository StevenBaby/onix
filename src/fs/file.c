#include <onix/fs.h>
#include <onix/task.h>
#include <onix/assert.h>

#define FILE_NR 128

file_t file_table[FILE_NR];
fs_op_t *fs_ops[FS_TYPE_NUM];

fs_op_t *fs_get_op(int type)
{
    assert(type > 0 && type < FS_TYPE_NUM);
    return fs_ops[type];
}

void fs_register_op(int type, fs_op_t *op)
{
    assert(type > 0 && type < FS_TYPE_NUM);
    fs_ops[type] = op;
}

int fs_default_nosys()
{
    return -ENOSYS;
}

void *fs_default_null()
{
    return NULL;
}

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

err_t fd_check(fd_t fd, file_t **file)
{
    task_t *task = running_task();
    if (fd >= TASK_FILE_NR || !task->files[fd])
        return -EINVAL;

    *file = task->files[fd];
    return EOK;
}

fd_t fd_get(file_t **file)
{
    task_t *task = running_task();
    fd_t fd;

    for (fd = 3; fd < TASK_FILE_NR; fd++)
    {
        if (!task->files[fd])
            break;
    }

    if (fd == TASK_FILE_NR)
    {
        return -EMFILE;
    };

    file_t *f = get_file();
    assert(task->files[fd] == NULL);
    task->files[fd] = f;
    *file = f;
    return fd;
}

err_t fd_put(fd_t fd)
{
    file_t *file;
    err_t ret = EOK;
    if ((ret = fd_check(fd, &file)) < EOK)
        return ret;

    task_t *task = running_task();
    assert(file->inode);
    task->files[fd] = NULL;
    put_file(file);
    return EOK;
}

void file_init()
{
    for (size_t i = 3; i < FILE_NR; i++)
    {
        file_t *file = &file_table[i];
        file->count = 0;
        file->flags = 0;
        file->offset = 0;
        file->inode = NULL;
    }
}
