#include <onix/syscall.h>
#include <onix/signal.h>
#include <onix/stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        return -1;
    }
    return kill(atoi(argv[1]), SIGTERM);
}