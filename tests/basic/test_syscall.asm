[bits 32]

section .text
global main
main:

    ; write(stdout, message, len)
    mov eax, 4; write
    mov ebx, 1; stdout
    mov ecx, message; buffer
    mov edx, message.end - message
    int 0x80

    ; exit(0)
    mov eax, 1; exit
    mov ebx, 0; status
    int 0x80

section .data
message:
     db "hello world!!!", 10, 13, 0
.end:
