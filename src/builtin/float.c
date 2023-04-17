#include <onix/stdio.h>
#include <onix/syscall.h>

int main(int argc, char const *argv[])
{
    float a = 1.0;
    while (true)
    {
        a += 1.1;
        printf("PID %d TIME %d float add....\n", getpid(), time());
        sleep(300);
    }
    return 0;
}
