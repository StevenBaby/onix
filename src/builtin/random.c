#include <onix/syscall.h>
#include <onix/stdlib.h>
#include <onix/stdio.h>

int main(int argc, char const *argv[])
{
  while (true)
  {
    // srand(time());
    printf("hello onix random %d\a\n", rand());
    sleep(1000);
  }
  return 0;
}
