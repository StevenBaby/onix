[bits 32]

extern console_init
extern memory_init
extern kernel_init

global _start
_start:
    push ebx; ards_count
    push eax; magic

    call console_init; 控制台初始化
    call memory_init; 内存初始化

    jmp $; 阻塞
