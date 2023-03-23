[bits 32]

extern ssetmask
section .text
global restorer

restorer:
    add esp, 4; sig
    call ssetmask
    add esp, 4; blocked
    ; 以下恢复调用方寄存器
    pop eax
    pop ecx
    pop edx
    popf
    ret
