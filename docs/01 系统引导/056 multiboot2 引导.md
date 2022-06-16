# multiboot2 引导

## i386 状态

- EAX：魔数 `0x36d76289`
- EBX：包含 bootloader 存储 multiboot2 信息结构体的，32 位 物理地址
- CS：32 位 可读可执行的代码段，尺寸 4G
- DS/ES/FS/GS/SS：32 位可读写的数据段，尺寸 4G
- A20 线：启用
- CR0：PG = 0, PE = 1，其他未定义
- EFLAGS：VM = 0, IF = 0, 其他未定义
- ESP：内核必须尽早切换栈顶地址
- GDTR：内核必须尽早使用自己的全局描述符表
- IDTR：内核必须在设置好自己的中断描述符表之前关闭中断

Virtual 8086 Mode

## 参考文献

- <https://www.gnu.org/software/grub/manual/multiboot2/multiboot.pdf>