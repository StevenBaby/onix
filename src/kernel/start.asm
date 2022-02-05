[bits 32]

extern kernel_init

global _start
_start:
    ; mov byte [0xb8000], 'K'; 表示进入了内核
    xchg bx, bx
    call kernel_init
    xchg bx, bx
    jmp $; 阻塞
