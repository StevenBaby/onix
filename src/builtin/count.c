#include <onix/syscall.h>
#include <onix/stdio.h>

int main(int argc, char const *argv[])
{
    int counter = 1;
    while (counter)
    {
        printf("hello onix %d\a\n", counter++);
        sleep(1000);
    }
    return 0;
}
