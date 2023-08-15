#include <onix/fs.h>
#include <onix/assert.h>
#include <onix/task.h>
#include <onix/device.h>
#include <onix/syscall.h>
#include <onix/memory.h>
#include <onix/debug.h>
#include <onix/string.h>
#include <onix/stat.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

fd_t sys_open(char *filename, int flags, int mode)
{
    char *next;
    inode_t *dir = NULL;

    dir = named(filename, &next);
    if (!dir)
    {
        return -ENOENT;
    }

    inode_t *inode = NULL;
    if (!*next)
    {
        inode = dir;
        dir->count++;
        goto makeup;
    }

    int ret = dir->op->open(dir, next, flags, mode, &inode);
    if (ret < 0)
        goto rollback;

makeup:
    file_t *file;
    fd_t fd = fd_get(&file);
    if (fd < EOK)
        return fd;

    file->inode = inode;
    file->flags = flags;
    file->count = 1;
    file->offset = 0;

    if (flags & O_APPEND)
    {
        file->offset = file->inode->size;
    }

    ret = fd;

rollback:
    iput(dir);
    return ret;
}

int sys_creat(char *filename, int mode)
{
    return sys_open(filename, O_CREAT | O_TRUNC, mode);
}

void sys_close(fd_t fd)
{
    fd_put(fd);
}

int sys_read(fd_t fd, char *buf, int count)
{
    if (count < 0)
        return -EINVAL;

    bool user = true;
    if (running_task()->uid == KERNEL_USER)
        user = false;

    if (!memory_access(buf, count, false, user))
        return -EINVAL;

    file_t *file;
    err_t ret = EOK;
    if ((ret = fd_check(fd, &file)) < EOK)
        return ret;

    if ((file->flags & O_ACCMODE) == O_WRONLY)
        return EOF;

    inode_t *inode = file->inode;

    int len = inode->op->read(inode, buf, count, file->offset);

    if (len > 0)
        file->offset += len;

    return len;
}

int sys_readdir(fd_t fd, dirent_t *dir, u32 count)
{
    bool user = true;
    if (running_task()->uid == KERNEL_USER)
        user = false;

    if (!memory_access(dir, sizeof(dirent_t), false, user))
        return -EINVAL;

    file_t *file;
    err_t ret = EOK;
    if ((ret = fd_check(fd, &file)) < EOK)
        return ret;

    if ((file->flags & O_ACCMODE) == O_WRONLY)
        return EOF;

    inode_t *inode = file->inode;

    int len = inode->op->readdir(inode, dir, count, file->offset);

    if (len > 0)
        file->offset += len;

    return len;
}

int sys_write(unsigned int fd, char *buf, int count)
{
    if (count < 0)
        return -EINVAL;

    bool user = true;
    if (running_task()->uid == KERNEL_USER)
        user = false;

    if (!memory_access(buf, count, true, user))
        return -EINVAL;

    file_t *file;
    err_t ret = EOK;
    if ((ret = fd_check(fd, &file)) < EOK)
        return ret;

    if ((file->flags & O_ACCMODE) == O_RDONLY)
        return -EPERM;

    inode_t *inode = file->inode;
    assert(inode);

    int len = inode->op->write(inode, buf, count, file->offset);

    if (len > 0)
        file->offset += len;

    return len;
}

int sys_lseek(fd_t fd, off_t offset, whence_t whence)
{
    err_t ret = EOK;
    file_t *file;
    if ((ret = fd_check(fd, &file)) < EOK)
        return ret;

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
        assert(file->inode->size + offset >= 0);
        file->offset = file->inode->size + offset;
        break;
    default:
        panic("whence not defined !!!");
        break;
    }
    return file->offset;
}

static int dupfd(fd_t fd, fd_t arg)
{
    int ret = EOK;
    file_t *file;
    if ((ret = fd_check(fd, &file)) < EOK)
        return ret;

    task_t *task = running_task();
    for (; arg < TASK_FILE_NR; arg++)
    {
        if (!task->files[arg])
            break;
    }

    if (arg >= TASK_FILE_NR)
        return -EINVAL;

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

int sys_stat(char *filename, stat_t *statbuf)
{
    inode_t *inode = namei(filename);
    if (!inode)
        return -ENOENT;

    int ret = inode->op->stat(inode, statbuf);
    iput(inode);
    return ret;
}

int sys_fstat(fd_t fd, stat_t *statbuf)
{
    file_t *file;
    err_t ret;
    if ((ret = fd_check(fd, &file)) < EOK)
        return ret;

    inode_t *inode = file->inode;
    assert(inode);
    return inode->op->stat(inode, statbuf);
}

// 控制设备输入输出
int sys_ioctl(fd_t fd, int cmd, void *args)
{
    if (fd >= TASK_FILE_NR)
        return -EBADF;

    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file)
        return -EBADF;

    inode_t *inode = file->inode;
    assert(inode);
    return inode->op->ioctl(inode, cmd, args);
}

char *sys_getcwd(char *buf, size_t size)
{
    task_t *task = running_task();
    strncpy(buf, task->pwd, size);
    return buf;
}

// 计算 当前路径 pwd 和新路径 pathname, 存入 pwd
void abspath(char *pwd, const char *pathname)
{
    char *cur = NULL;
    char *ptr = NULL;
    if (IS_SEPARATOR(pathname[0]))
    {
        cur = pwd + 1;
        *cur = 0;
        pathname++;
    }
    else
    {
        cur = strrsep(pwd) + 1;
        *cur = 0;
    }

    while (pathname[0])
    {
        ptr = strsep(pathname);
        if (!ptr)
        {
            break;
        }

        int len = (ptr - pathname) + 1;
        *ptr = '/';
        if (!memcmp(pathname, "./", 2))
        {
            /* code */
        }
        else if (!memcmp(pathname, "../", 3))
        {
            if (cur - 1 != pwd)
            {
                *(cur - 1) = 0;
                cur = strrsep(pwd) + 1;
                *cur = 0;
            }
        }
        else
        {
            strncpy(cur, pathname, len + 1);
            cur += len;
        }
        pathname += len;
    }

    if (!pathname[0])
        return;

    if (!strcmp(pathname, "."))
        return;

    if (strcmp(pathname, ".."))
    {
        strcpy(cur, pathname);
        cur += strlen(pathname);
        *cur = '/';
        *(cur + 1) = '\0';
        return;
    }
    if (cur - 1 != pwd)
    {
        *(cur - 1) = 0;
        cur = strrsep(pwd) + 1;
        *cur = 0;
    }
}

int sys_chdir(char *pathname)
{
    inode_t *inode = namei(pathname);
    if (!inode)
        return -ENOENT;

    assert(inode->dev > 0);

    int ret = -ERROR;
    task_t *task = running_task();

    if (inode == task->ipwd)
    {
        ret = EOK;
        goto rollback;
    }

    if (!ISDIR(inode->mode))
    {
        ret = -EPERM;
        goto rollback;
    }

    abspath(task->pwd, pathname);

    iput(task->ipwd);
    task->ipwd = inode;
    return EOK;

rollback:
    iput(inode);
    return ret;
}

int sys_chroot(char *pathname)
{

    inode_t *inode = namei(pathname);
    if (!inode)
        return -ENOENT;

    task_t *task = running_task();
    err_t ret;

    if (!ISDIR(inode->mode) || inode == task->iroot)
    {
        ret = -EPERM;
        goto rollback;
    }

    iput(task->iroot);
    task->iroot = inode;
    return EOK;

rollback:
    iput(inode);
    return ret;
}

int sys_mkdir(char *pathname, int mode)
{
    char *next = NULL;
    inode_t *dir = NULL;
    int ret = -ERROR;

    dir = named(pathname, &next);
    if (!dir) // 父目录不存在
    {
        ret = -ENOENT;
        goto rollback;
    }

    // 目录名为空
    if (!*next)
    {
        ret = -ENOENT;
        goto rollback;
    }

    // 父目录无写权限
    if (!dir->op->permission(dir, P_WRITE))
    {
        ret = -EPERM;
        goto rollback;
    }

    ret = dir->op->mkdir(dir, next, mode);

rollback:
    iput(dir);
    return ret;
}

int sys_rmdir(char *pathname)
{
    char *next = NULL;
    inode_t *dir = NULL;
    int ret = -ERROR;

    dir = named(pathname, &next);
    if (!dir) // 父目录不存在
    {
        ret = -ENOENT;
        goto rollback;
    }

    // 目录名为空
    if (!*next)
    {
        ret = -ENOENT;
        goto rollback;
    }

    // 父目录无写权限
    if (!dir->op->permission(dir, P_WRITE))
    {
        ret = -EPERM;
        goto rollback;
    }

    ret = dir->op->rmdir(dir, next);

rollback:
    iput(dir);
    return ret;
}

int sys_link(char *oldname, char *newname)
{
    int ret = -ERROR;

    inode_t *odir = NULL;
    char *oname = NULL;
    odir = named(oldname, &oname);
    if (!odir)
    {
        ret = -ENOENT;
        goto rollback;
    }

    inode_t *ndir = NULL;
    char *nname = NULL;
    ndir = named(newname, &nname);
    if (!ndir)
    {
        ret = -ENOENT;
        goto rollback;
    }
    ret = ndir->op->link(odir, oname, ndir, nname);
rollback:
    iput(odir);
    iput(ndir);
    return ret;
}

int sys_unlink(char *filename)
{
    int ret = -ERROR;
    char *next = NULL;

    inode_t *dir = named(filename, &next);
    if (!dir)
        return -ENOENT;

    if (!(*next))
    {
        ret = -ENOENT;
        goto rollback;
    }

    if (!dir->op->permission(dir, P_WRITE))
    {
        ret = -EPERM;
        goto rollback;
    }

    ret = dir->op->unlink(dir, next);

rollback:
    iput(dir);
    return ret;
}

int sys_mknod(char *filename, int mode, int dev)
{
    int ret = -ERROR;
    char *next = NULL;
    inode_t *dir = named(filename, &next);
    if (!dir)
        return -ENOENT;

    if (!(*next))
    {
        ret = -ENOENT;
        goto rollback;
    }

    if (!dir->op->permission(dir, P_WRITE))
    {
        ret = -EPERM;
        goto rollback;
    }

    ret = dir->op->mknod(dir, next, mode, dev);

rollback:
    iput(dir);
    return ret;
}

int sys_mount(char *devname, char *dirname, int flags)
{
    LOGK("mount %s to %s\n", devname, dirname);

    inode_t *devinode = NULL;
    inode_t *dirinode = NULL;
    super_t *super = NULL;
    int ret = -ERROR;

    devinode = namei(devname);
    if (!devinode)
    {
        ret = -ENOENT;
        goto rollback;
    }

    if (!ISBLK(devinode->mode))
    {
        ret = -EPERM;
        goto rollback;
    }

    if (devinode->rdev == 0)
    {
        ret = -ENOSYS;
        goto rollback;
    }

    dirinode = namei(dirname);
    if (!dirinode)
    {
        ret = -ENOENT;
        goto rollback;
    }

    if (!ISDIR(dirinode->mode))
    {
        ret = -EPERM;
        goto rollback;
    }

    if (dirinode->count != 1 || dirinode->mount)
    {
        ret = -EBUSY;
        goto rollback;
    }

    super = read_super(devinode->rdev);
    if (!super)
    {
        ret = -EFSUNK;
        goto rollback;
    }

    if (super->imount)
    {
        ret = -EBUSY;
        goto rollback;
    }

    super->imount = dirinode;
    dirinode->mount = devinode->rdev;
    iput(devinode);
    return EOK;

rollback:
    put_super(super);
    iput(devinode);
    iput(dirinode);
    return ret;
}

int sys_umount(char *target)
{
    LOGK("umount %s\n", target);
    inode_t *inode = NULL;
    super_t *super = NULL;
    int ret = -ERROR;

    inode = namei(target);
    if (!inode)
    {
        ret = -ENOENT;
        goto rollback;
    }

    // 如果传入不是块设备文件，而且不是设备根目录
    if (!ISBLK(inode->mode) && (inode->super->iroot != inode))
    {
        ret = -ENOTBLK;
        goto rollback;
    }

    // 系统根目录不允许释放
    if (inode == get_root_inode())
    {
        ret = -EBUSY;
        goto rollback;
    }

    dev_t dev = inode->dev;
    if (ISBLK(inode->mode))
    {
        dev = inode->rdev;
    }

    super = read_super(dev); // 使用 read 使得 count++
    assert(super);
    if (!super->imount)
    {
        ret = -ENOENT;
        goto rollback;
    }

    if (!super->imount->mount)
    {
        LOGK("warning super block mount = 0\n");
    }

    // 根目录引用还有其他人使用根目录
    if (super->iroot->count > 2)
    {
        ret = -EBUSY;
        goto rollback;
    }

    if (super->iroot->count == 2 && inode->super->iroot != inode)
    {
        ret = -EBUSY;
        goto rollback;
    }

    if (list_size(&super->inode_list) > 1)
    {
        ret = -EBUSY;
        goto rollback;
    }

    iput(super->iroot);
    super->iroot = NULL;

    super->imount->mount = 0;
    iput(super->imount);
    super->imount = NULL;
    super->count--;
    assert(super->count == 1);
    ret = EOK;

rollback:
    iput(inode);
    put_super(super);
    return ret;
}

int sys_mkfs(char *devname, int args)
{
    inode_t *inode = NULL;
    int ret = EOF;

    inode = namei(devname);
    if (!inode)
    {
        ret = -ENOENT;
        goto rollback;
    }

    if (!ISBLK(inode->mode))
    {
        ret = -ENOTDIR;
        goto rollback;
    }

    dev_t dev = inode->rdev;
    assert(dev);
    ret = inode->op->mkfs(dev, args);

rollback:
    iput(inode);
    return ret;
}
