#include <onix/types.h>
#include <onix/syscall.h>
#include <onix/stdio.h>
#include <onix/task.h>

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

extern void keyboard_init();
extern void time_init();
extern void tty_init();
extern void rtc_init();

extern void ide_init();
extern void floppy_init();
extern void ramdisk_init();
extern void sb16_init();
extern void e1000_init();
extern void mouse_init();

extern void buffer_init();
extern void file_init();
extern void inode_init();
extern void pipe_init();
extern void minix_init();
extern void iso_init();
extern void super_init();
extern void dev_init();
extern void net_init();
extern void resolv_init();
extern void vm86_init();
extern void vga_init();
extern void vesa_init();

void init_thread()
{
    keyboard_init(); // 初始化键盘
    time_init();     // 初始化时间
    tty_init();      // 初始化 TTY 设备，必须在键盘之后
    // rtc_init();   // 初始化实时时钟，目前没用

    ramdisk_init(); // 初始化内存虚拟磁盘

    ide_init();    // 初始化 IDE 设备
    sb16_init();   // 初始化声霸卡
    floppy_init(); // 初始化软盘
    e1000_init();  // 初始化 e1000 网卡
    mouse_init();  // 初始化鼠标

    buffer_init(); // 初始化高速缓冲
    file_init();   // 初始化文件
    inode_init();  // 初始化 inode
    minix_init();  // 初始化 minix 文件系统
    iso_init();    // 初始化 iso9660 文件系统
    pipe_init();   // 初始化管道
    super_init();  // 初始化超级块

    dev_init();    // 初始化设备文件
    net_init();    // 配置网卡
    resolv_init(); // 初始化域名解析
    vm86_init();   // 初始化 VM8086
    vga_init();    // 初始化 VGA
    vesa_init();   // 初始化 VESA

    task_to_user_mode(); // 进入用户态
}