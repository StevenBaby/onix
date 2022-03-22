[bits 32]

extern console_init
extern gdt_init
extern memory_init
extern kernel_init

global _start
_start:
    push ebx; ards_count
    push eax; magic

    call console_init   ; 控制台初始化
    call gdt_init       ; 全局描述符初始化
    call memory_init    ; 内存初始化
    call kernel_init    ; 内核初始化

    jmp $; 阻塞
