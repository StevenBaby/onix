#include <onix/stdio.h>

int main()
{
    char *video = (char *)0xB8000;
    printf("char in 0x%X is %c\n", video, *video);
    *video = 'E';
    printf("changed to E\n");
    return 0;
}
