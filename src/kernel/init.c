#include <onix/types.h>
#include <onix/syscall.h>
#include <onix/stdio.h>
#include <onix/task.h>

// 进入用户态，准备内核栈
// 保证栈顶占用足够的大小
// 约等于 char temp[100];
static void prepare_stack()
{
    // 获取返回地址，也就是下面调用 task_to_user_mode 的位置
    void *addr = __builtin_return_address(0);
    asm volatile(
        "subl $100, %%esp\n" // 为栈顶有足够的空间
        "pushl %%eax\n"      // 将返回地址压入栈中
        "ret \n"             // 直接返回
        ::"a"(addr));
}

extern int main();

int init_user_thread()
{
    while (true)
    {
        u32 status;
        pid_t pid = fork();
        if (pid)
        {
            pid_t child = waitpid(pid, &status);
            printf("wait pid %d status %d %d\n", child, status, time());
        }
        else
        {
            main();
        }
    }
    return 0;
}

extern void serial_init();
extern void keyboard_init();
extern void time_init();
extern void tty_init();
extern void rtc_init();

extern void ide_init();
extern void floppy_init();
extern void ramdisk_init();
extern void sb16_init();

extern void buffer_init();
extern void file_init();
extern void inode_init();
extern void super_init();
extern void dev_init();

void init_thread()
{
    serial_init();   // 初始化串口
    keyboard_init(); // 初始化键盘
    time_init();     // 初始化时间
    tty_init();      // 初始化 TTY 设备，必须在键盘之后
    // rtc_init();   // 初始化实时时钟，目前没用

    ramdisk_init(); // 初始化内存虚拟磁盘

    ide_init();    // 初始化 IDE 设备
    sb16_init();   // 初始化声霸卡
    floppy_init(); // 初始化软盘

    buffer_init(); // 初始化高速缓冲
    file_init();   // 初始化文件
    inode_init();  // 初始化 inode
    super_init();  // 初始化超级块

    dev_init(); // 初始化设备文件

    prepare_stack();     // 准备栈顶
    task_to_user_mode(); // 进入用户态
}