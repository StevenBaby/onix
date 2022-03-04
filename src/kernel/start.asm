[bits 32]

extern kernel_init

global _start
_start:
    call kernel_init
    xchg bx, bx
    int 0x80; 调用 0x80 中断函数 系统调用

    ; mov bx, 0
    ; div bx

    jmp $; 阻塞
