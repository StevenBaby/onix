#include <onix/errno.h>
#include <onix/types.h>

typedef struct strerror_t
{
    int errno;
    const char *str;
} strerror_t;

static strerror_t strerror_table[] = {
    {EPERM, "Operation not permitted"},
    {ENOENT, "No such file or directory"},
    {ESRCH, "No such process"},
    {EINTR, "Interrupted system call"},
    {EIO, "I/O error"},
    {ENXIO, "No such device or address"},
    {E2BIG, "Argument list too long"},
    {ENOEXEC, "Exec format error"},
    {EBADF, "Bad file number"},
    {ECHILD, "No child processes"},
    {EAGAIN, "Try again"},
    {ENOMEM, "Out of memory"},
    {EACCES, "Permission denied"},
    {EFAULT, "Bad address"},
    {ENOTBLK, "Block device required"},
    {EBUSY, "Device or resource busy"},
    {EEXIST, "Directory entry already exists"},
    {EXDEV, "Cross-device link"},
    {ENODEV, "No such device"},
    {ENOTDIR, "Not a directory"},
    {EISDIR, "Is a directory"},
    {EINVAL, "Invalid argument"},
    {ENFILE, "File table overflow"},
    {EMFILE, "Too many open files"},
    {ENOTTY, "Not a typewriter"},
    {ETXTBSY, "Text file busy"},
    {EFBIG, "File too large"},
    {ENOSPC, "No space left on device"},
    {ESPIPE, "Illegal seek"},
    {EROFS, "Read-only file system"},
    {EMLINK, "Too many links"},
    {EPIPE, "Broken pipe"},
    {EDOM, "Math argument out of domain of func"},
    {ERANGE, "Math result not representable"},
    {EDEADLK, "Resource deadlock would occur"},
    {ENAMETOOLONG, "File name too long"},
    {ENOLCK, "No record locks available"},
    {ENOSYS, "Invalid system call number"},
    {ENOTEMPTY, "Directory not empty"},
    {ETIME, "Timer expired"},
    {EFSUNK, "File system unknown"},
    {EMSGSIZE, "Message too long"},
    {ERROR, "General error"},
    {EEOF, "End of file"},

    {EADDR, "Address error"},
    {EPROTO, "Protocol error"},
    {EOPTION, "Option error"},
    {EFRAG, "Fragment error"},
    {ESOCKET, "Socket error"},
    {EOCCUPIED, "Connection occupied error"},
    {ENOTCONN, "Not connection error"},
    {ERESET, "Reset error"},
    {ECHKSUM, "Chksum error"},
};

const char *strerror(int errno)
{
    errno = -errno;
    if (errno <= 122)
    {
        for (size_t i = 0; i < 115; i++)
        {
            strerror_t *err = &strerror_table[i];
            if (err->errno == errno)
            {
                return err->str;
            }
        }
    }
    return "Unknown Error";
}