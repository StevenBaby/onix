[bits 32]

section .text
global _start

_start:
    ; write(stdout, message , sizeof(message))
    mov eax, 4; write
    mov ebx, 1; stdout
    mov ecx, message
    mov edx, message.end - message - 1
    int 0x80

    ; exit(0);
    mov eax, 1; nr_exit
    mov ebx, 0; code
    int 0x80

section .data

message:
    db "hello onix!!!", 10, 0
    .end:

section .bss

buffer: resb 1024; 预留 1024 个字节的空间
