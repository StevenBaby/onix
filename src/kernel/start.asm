[bits 32]

extern kernel_init

global _start
_start:
    call kernel_init
    jmp $; 阻塞
