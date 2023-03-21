#include <onix/types.h>
#include <onix/errno.h>
#include <onix/stat.h>
#include <onix/task.h>
#include <onix/device.h>
#include <onix/fs.h>

// 控制设备输入输出
int sys_ioctl(fd_t fd, int cmd, void *args)
{
    if (fd >= TASK_FILE_NR)
        return -EBADF;

    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file)
        return -EBADF;

    // 文件必须是某种设备
    int mode = file->inode->desc->mode;
    if (!ISCHR(mode) && !ISBLK(mode))
        return -EINVAL;

    // 得到设备号
    dev_t dev = file->inode->desc->zone[0];
    if (dev >= DEVICE_NR)
        return -ENOTTY;

    // 进行设备控制
    return device_ioctl(dev, cmd, args, 0);
}
