#include <onix/fs.h>
#include <onix/stat.h>
#include <onix/task.h>
#include <onix/assert.h>

static void copy_stat(inode_t *inode, stat_t *statbuf)
{
    statbuf->dev = inode->dev;             // 文件所在的设备号
    statbuf->nr = inode->nr;               // 文件 i 节点号
    statbuf->mode = inode->desc->mode;     // 文件属性
    statbuf->nlinks = inode->desc->nlinks; // 文件的连接数
    statbuf->uid = inode->desc->uid;       // 文件的用户 id
    statbuf->gid = inode->desc->gid;       // 文件的组 id
    statbuf->rdev = inode->desc->zone[0];  // 设备号(如果文件是特殊的字符文件或块文件)
    statbuf->size = inode->desc->size;     // 文件大小（字节数）（如果文件是常规文件）
    statbuf->atime = inode->atime;         // 最后访问时间
    statbuf->mtime = inode->desc->mtime;   // 最后修改时间
    statbuf->ctime = inode->ctime;         // 最后节点修改时间
}

int sys_stat(char *filename, stat_t *statbuf)
{
    inode_t *inode = namei(filename);

    if (!inode)
    {
        return EOF;
    }

    copy_stat(inode, statbuf);
    iput(inode);
    return 0;
}

int sys_fstat(fd_t fd, stat_t *statbuf)
{

    if (fd >= TASK_FILE_NR)
    {
        return EOF;
    }
    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file)
    {
        return EOF;
    }

    inode_t *inode = file->inode;
    assert(inode);

    copy_stat(inode, statbuf);
    return 0;
}
