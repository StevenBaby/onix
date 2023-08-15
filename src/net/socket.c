#include <onix/net/socket.h>
#include <onix/net/netif.h>
#include <onix/fs.h>
#include <onix/task.h>
#include <onix/arena.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/memory.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define BACKLOG 32

static socket_op_t *socket_ops[SOCK_TYPE_NUM];

static socket_op_t *socket_get_op(socktype_t type)
{
    assert(type > 0 && type < SOCK_TYPE_NUM);
    return socket_ops[type];
}

void socket_register_op(socktype_t type, socket_op_t *op)
{
    assert(type > 0 && type < SOCK_TYPE_NUM);
    socket_ops[type] = op;
}

static inode_t *socket_inode_create()
{
    inode_t *inode = get_free_inode();
    // 区别于 EOF 这里是无效的设备，但是被占用了
    inode->dev = -FS_TYPE_SOCKET;
    inode->desc = NULL;

    inode->count = 1;
    inode->type = FS_TYPE_SOCKET;
    inode->op = fs_get_op(FS_TYPE_SOCKET);

    return inode;
}

socket_t *socket_create()
{
    socket_t *s = kmalloc(sizeof(socket_t));
    memset(s, 0, sizeof(socket_t));
    s->rcvtimeo = TIMELESS;
    s->sndtimeo = TIMELESS;
    return s;
}

static inode_t *socket_open()
{
    inode_t *inode = socket_inode_create();
    inode->desc = (void *)socket_create();
    return inode;
}

static void socket_close(inode_t *inode)
{
    if (!inode)
        return;
    inode->count--;
    if (inode->count)
        return;
    inode->type = FS_TYPE_NONE;

    assert(inode->desc);
    socket_t *s = (socket_t *)inode->desc;

    if (s->type != SOCK_TYPE_NONE)
    {
        socket_get_op(s->type)->close(s);
    }

    kfree(inode->desc);

    // 释放 inode
    put_free_inode(inode);

    LOGK("socket close...\n");
}

static socket_t *socket_get(fd_t fd)
{
    task_t *task = running_task();
    file_t *file = task->files[fd];
    if (!file)
        return NULL;

    inode_t *inode = file->inode;
    if (!inode)
        return NULL;

    if (!inode->desc)
        return NULL;

    return (socket_t *)inode->desc;
}

int sys_socket(int domain, int type, int protocol)
{
    LOGK("sys_socket...\n");

    socktype_t socktype = SOCK_TYPE_NONE;
    switch (domain)
    {
    case AF_INET:
        switch (type)
        {
        case SOCK_RAW:
            socktype = SOCK_TYPE_RAW;
            break;
        case SOCK_DGRAM:
            socktype = SOCK_TYPE_UDP;
            break;
        case SOCK_STREAM:
            socktype = SOCK_TYPE_TCP;
            break;
        default:
            return -EINVAL;
            break;
        }
        break;
    case AF_PACKET:
        socktype = SOCK_TYPE_PKT;
        break;
    default:
        return -EINVAL;
        break;
    }

    file_t *file;
    fd_t fd = fd_get(&file);
    if (fd < EOK)
        return fd;

    inode_t *inode = socket_open();

    file->inode = inode;
    file->flags = 0;
    file->count = 1;
    file->offset = 0;

    socket_t *s = (socket_t *)inode->desc;
    s->type = socktype;

    if (s->type != SOCK_TYPE_NONE)
    {
        socket_get_op(s->type)->socket(s, domain, type, protocol);
    }

    return fd;
}

int sys_listen(int fd, int backlog)
{
    if (backlog <= 0 || backlog > BACKLOG)
        return -EINVAL;

    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    return socket_get_op(s->type)->listen(s, backlog);
}

int sys_accept(int fd, sockaddr_t *addr, int *addrlen)
{
    LOGK("sys_accept...\n");
    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    socket_t *ns = NULL;
    int ret = socket_get_op(s->type)->accept(s, addr, addrlen, &ns);
    if (ret < 0)
        return ret;

    inode_t *inode = socket_inode_create();
    inode->desc = ns;

    file_t *file;
    fd = fd_get(&file);

    file->inode = inode;
    file->flags = 0;
    file->count = 1;
    file->offset = 0;
    return fd;
}

int sys_bind(int fd, const sockaddr_t *name, int namelen)
{
    if (!name)
        return -EFAULT;
    if (namelen < sizeof(sockaddr_in_t))
        return -EFAULT;

    sockaddr_in_t *sin = (sockaddr_in_t *)name;
    switch (sin->family)
    {
    case AF_INET:
    case AF_UNSPEC:
    case AF_PACKET:
        break;
    default:
        return -EPROTO;
        break;
    }

    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    return socket_get_op(s->type)->bind(s, name, namelen);
}

int sys_connect(int fd, const sockaddr_t *name, int namelen)
{
    if (!name)
        return -EFAULT;
    if (namelen < sizeof(sockaddr_in_t))
        return -EFAULT;

    sockaddr_in_t *sin = (sockaddr_in_t *)name;
    switch (sin->family)
    {
    case AF_INET:
    case AF_UNSPEC:
    case AF_PACKET:
        break;
    default:
        return -EPROTO;
        break;
    }

    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    return socket_get_op(s->type)->connect(s, name, namelen);
}

int sys_shutdown(int fd, int how)
{
    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    return socket_get_op(s->type)->shutdown(s, how);
}

int sys_getpeername(int fd, sockaddr_t *name, int *namelen)
{
    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    return socket_get_op(s->type)->getpeername(s, name, namelen);
}

int sys_getsockname(int fd, sockaddr_t *name, int *namelen)
{
    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    return socket_get_op(s->type)->getsockname(s, name, namelen);
}

int sys_getsockopt(int fd, int level, int optname, void *optval, int *optlen)
{
    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    return socket_get_op(s->type)->getsockopt(s, level, optname, optval, optlen);
}

int sys_setsockopt(int fd, int level, int optname, const void *optval, int optlen)
{
    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    if (level == SOL_SOCKET)
    {
        switch (optname)
        {
        case SO_SNDTIMEO:
            if (optlen != 4)
                return -EINVAL;
            s->sndtimeo = *(int *)optval;
            return EOK;
        case SO_RCVTIMEO:
            if (optlen != 4)
                return -EINVAL;
            s->rcvtimeo = *(int *)optval;
            return EOK;
        default:
            break;
        }
    }

    return socket_get_op(s->type)->setsockopt(s, level, optname, optval, optlen);
}

int sys_recvfrom(int fd, void *data, int size, u32 flags, sockaddr_t *from, int *fromlen)
{
    if (!data)
        return -EFAULT;
    if (size < 0)
        return -EINVAL;

    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    msghdr_t msg;
    iovec_t iov;

    msg.name = from;
    msg.namelen = fromlen ? *fromlen : 0;

    msg.iov = &iov;
    msg.iovlen = 1;

    iov.base = data;
    iov.size = size;

    int ret = iovec_check(msg.iov, msg.iovlen, true);
    if (ret < EOK)
        return ret;

    ret = socket_get_op(s->type)->recvmsg(s, &msg, flags);

    if (fromlen)
        *fromlen = msg.namelen;

    return ret;
}

int sys_recvmsg(int fd, msghdr_t *msg, u32 flags)
{
    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    msghdr_t m;
    memcpy(&m, msg, sizeof(msghdr_t));
    m.iov = iovec_dup(msg->iov, msg->iovlen);

    int ret = iovec_check(m.iov, m.iovlen, true);
    if (ret < EOK)
        return ret;

    ret = socket_get_op(s->type)->recvmsg(s, &m, flags);

    kfree(m.iov);
    return ret;
}

int sys_sendto(int fd, void *data, int size, u32 flags, sockaddr_t *to, int tolen)
{
    if (!data)
        return -EFAULT;
    if (size < 0)
        return -EINVAL;

    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    msghdr_t msg;
    iovec_t iov;

    msg.name = to;
    msg.namelen = tolen;

    msg.iov = &iov;
    msg.iovlen = 1;

    iov.base = data;
    iov.size = size;

    int ret = iovec_check(msg.iov, msg.iovlen, false);
    if (ret < EOK)
        return ret;

    return socket_get_op(s->type)->sendmsg(s, &msg, flags);
}

int sys_sendmsg(int fd, msghdr_t *msg, u32 flags)
{
    socket_t *s = socket_get(fd);
    if (!s)
        return -EINVAL;

    msghdr_t m;
    memcpy(&m, msg, sizeof(msghdr_t));
    m.iov = iovec_dup(msg->iov, msg->iovlen);

    int ret = iovec_check(m.iov, m.iovlen, false);
    if (ret < EOK)
        return ret;

    ret = socket_get_op(s->type)->sendmsg(s, &m, flags);

    kfree(m.iov);
    return ret;
}

static int socket_read(inode_t *inode, char *data, int size, off_t offset)
{
    if (!data)
        return -EFAULT;
    if (size < 0)
        return -EINVAL;

    socket_t *s = (socket_t *)inode->desc;
    if (!s)
        return -EINVAL;

    msghdr_t msg;
    iovec_t iov;

    msg.name = NULL;
    msg.namelen = 0;

    msg.iov = &iov;
    msg.iovlen = 1;

    iov.base = data;
    iov.size = size;

    return socket_get_op(s->type)->recvmsg(s, &msg, 0);
}

static int socket_write(inode_t *inode, char *data, int size, off_t offset)
{
    if (!data)
        return -EFAULT;
    if (size < 0)
        return -EINVAL;

    socket_t *s = (socket_t *)inode->desc;
    if (!s)
        return -EINVAL;

    msghdr_t msg;
    iovec_t iov;

    msg.name = NULL;
    msg.namelen = 0;

    msg.iov = &iov;
    msg.iovlen = 1;

    iov.base = data;
    iov.size = size;

    return socket_get_op(s->type)->sendmsg(s, &msg, 0);
}

static int socket_ioctl(inode_t *inode, int cmd, void *args)
{
    if (!memory_access(args, sizeof(ifreq_t), true, true))
        return -EINVAL;

    ifreq_t *req = (ifreq_t *)args;
    netif_t *netif = NULL;
    switch (cmd)
    {
    case SIOCGIFNAME:
        netif = netif_get(req->index);
        if (!netif)
            return -EINVAL;
        strcpy(req->name, netif->name);
        return EOK;
    case SIOCGIFINDEX:
        netif = netif_found(req->name);
        if (!netif)
            return -EINVAL;
        req->index = netif->index;
        return EOK;
    default:
        break;
    }

    netif = netif_found(req->name);
    if (!netif)
        return -EINVAL;
    return netif_ioctl(netif, cmd, args, 0);
}

static fs_op_t socket_op = {
    fs_default_nosys,
    fs_default_nosys,

    fs_default_nosys,
    socket_close,

    socket_ioctl,
    socket_read,
    socket_write,
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

void socket_init()
{
    fs_register_op(FS_TYPE_SOCKET, &socket_op);
}
