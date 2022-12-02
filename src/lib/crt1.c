#include <onix/types.h>
#include <onix/syscall.h>
#include <onix/string.h>

int main(int argc, char **argv, char **envp);

// libc 构造函数
weak void _init()
{
}

// libc 析构函数
weak void _fini()
{
}

int __libc_start_main(
    int (*main)(int argc, char **argv, char **envp),
    int argc, char **argv,
    void (*_init)(),
    void (*_fini)(),
    void (*ldso)(), // 动态连接器
    void *stack_end)
{
    char **envp = argv + argc + 1;
    _init();
    int i = main(argc, argv, envp);
    _fini();
    exit(i);
}
