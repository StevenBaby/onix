#include <onix/interrupt.h>

extern void tss_init();
extern void memory_map_init();
extern void mapping_init();
extern void arena_init();

extern void interrupt_init();
extern void clock_init();
extern void timer_init();
extern void syscall_init();
extern void task_init();
extern void fpu_init();
extern void pci_init();

extern void pbuf_init();
extern void netif_init();
extern void loopif_init();
extern void eth_init();
extern void arp_init();
extern void ip_init();
extern void icmp_init();
extern void udp_init();
extern void tcp_init();

extern void dhcp_init();

extern void socket_init();
extern void pkt_init();
extern void raw_init();

void kernel_init()
{
    tss_init();        // 初始化任务状态段
    memory_map_init(); // 初始化物理内存数组
    mapping_init();    // 初始化内存映射
    arena_init();      // 初始化内核堆内存

    interrupt_init(); // 初始化中断
    timer_init();     // 初始化定时器
    clock_init();     // 初始化时钟
    fpu_init();       // 初始化 FPU 浮点运算单元
    pci_init();       // 初始化 PCI 总线

    syscall_init(); // 初始化系统调用
    task_init();    // 初始化任务

    pbuf_init();   // 初始化 pbuf
    netif_init();  // 初始化 netif
    loopif_init(); // 初始化 loopif
    eth_init();    // 初始化 Ethernet 协议
    arp_init();    // 初始化 ARP 协议
    ip_init();     // 初始化 IP 协议
    icmp_init();   // 初始化 ICMP 协议
    udp_init();    // 初始化 UDP 协议
    tcp_init();    // 初始化 TCP 协议

    dhcp_init(); // 初始化 DHCP 协议

    socket_init(); // 初始化 socket
    pkt_init();    // 初始化 pkt
    raw_init();    // 初始化原始套接字

    set_interrupt_state(true);
}
