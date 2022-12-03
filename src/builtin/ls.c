#include <onix/types.h>
#include <onix/stdio.h>
#include <onix/syscall.h>
#include <onix/string.h>
#include <onix/fs.h>
#include <onix/time.h>

#define BUF_LEN 1024

static char buf[BUF_LEN];

static void strftime(time_t stamp, char *buf)
{
    tm time;
    localtime(stamp, &time);
    sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d",
            time.tm_year + 1900,
            time.tm_mon,
            time.tm_mday,
            time.tm_hour,
            time.tm_min,
            time.tm_sec);
}

static void parsemode(int mode, char *buf)
{
    memset(buf, '-', 10);
    buf[10] = '\0';
    char *ptr = buf;

    switch (mode & IFMT)
    {
    case IFREG:
        *ptr = '-';
        break;
    case IFBLK:
        *ptr = 'b';
        break;
    case IFDIR:
        *ptr = 'd';
        break;
    case IFCHR:
        *ptr = 'c';
        break;
    case IFIFO:
        *ptr = 'p';
        break;
    case IFLNK:
        *ptr = 'l';
        break;
    case IFSOCK:
        *ptr = 's';
        break;
    default:
        *ptr = '?';
        break;
    }
    ptr++;

    for (int i = 6; i >= 0; i -= 3)
    {
        int fmt = (mode >> i) & 07;
        if (fmt & 0b100)
        {
            *ptr = 'r';
        }
        ptr++;
        if (fmt & 0b010)
        {
            *ptr = 'w';
        }
        ptr++;
        if (fmt & 0b001)
        {
            *ptr = 'x';
        }
        ptr++;
    }
}

char qualifies[] = {'B', 'K', 'M', 'G', 'T'};

void reckon_size(int *size, char *qualifer)
{
    int num = *size;
    int i = 0;
    *qualifer = 'B';

    while (num)
    {
        *qualifer = qualifies[i];
        *size = num;
        num >>= 10; // num /= 1024
        i++;
    }
}

int main(int argc, char const *argv[], char const *envp[])
{
    fd_t fd = open(".", O_RDONLY, 0);
    if (fd == EOF)
        return EOF;

    bool list = false;
    if (argc == 2 && !strcmp(argv[1], "-l"))
        list = true;

    lseek(fd, 0, SEEK_SET);
    dentry_t entry;
    while (true)
    {
        int len = readdir(fd, &entry, 1);
        if (len == EOF)
            break;
        if (!entry.nr)
            continue;
        if (!strcmp(entry.name, ".") || !strcmp(entry.name, ".."))
        {
            continue;
        }
        if (!list)
        {
            printf("%s ", entry.name);
            continue;
        }

        stat_t statbuf;

        stat(entry.name, &statbuf);

        parsemode(statbuf.mode, buf);
        printf("%s ", buf);

        strftime(statbuf.ctime, buf);

        int size = statbuf.size;
        char qualifier;
        reckon_size(&size, &qualifier);

        printf("% 2d % 2d % 2d % 4d%c %s %s\n",
               statbuf.nlinks,
               statbuf.uid,
               statbuf.gid,
               size,
               qualifier,
               buf,
               entry.name);
    }
    if (!list)
        printf("\n");
    close(fd);
    return 0;
}
