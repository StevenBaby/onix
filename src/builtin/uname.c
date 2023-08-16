#include <onix/types.h>
#include <onix/stdio.h>
#include <onix/syscall.h>
#include <onix/uname.h>
#include <onix/string.h>

int main(int argc, char const *argv[], char const *envp[])
{
    utsname_t name;
    int ret = uname(&name);
    if (ret < 0)
    {
        printf(strerror(ret));
        return ret;
    }
    printf("%s_%s\n", name.sysname, name.version);
    return 0;
}
