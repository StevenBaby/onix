#include <onix/uname.h>
#include <onix/memory.h>
#include <onix/task.h>
#include <onix/string.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

int sys_uname(utsname_t *buf)
{
    if (!memory_access(buf, sizeof(utsname_t), true, running_task()->uid))
        return -EINVAL;

    strncpy(buf->sysname, "onix", 9);
    strncpy(buf->nodename, "onix", 9);
    strncpy(buf->release, "release", 9);
    strncpy(buf->version, ONIX_VERSION, 9);
    strncpy(buf->machine, "machine", 9);
    return EOK;
}