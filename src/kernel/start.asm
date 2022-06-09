[bits 32]

magic   equ 0xe85250d6
i386    equ 0
length  equ header_end - header_start

section .multiboot2
header_start:
    dd magic  ; 魔数
    dd i386   ; 32位保护模式
    dd length ; 头部长度
    dd -(magic + i386 + length); 校验和

    ; 结束标记
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
header_end:

extern console_init
extern gdt_init
extern memory_init
extern kernel_init

section .text
global _start
_start:
    push ebx; ards_count
    push eax; magic

    call console_init   ; 控制台初始化
    call gdt_init       ; 全局描述符初始化
    call memory_init    ; 内存初始化
    call kernel_init    ; 内核初始化

    jmp $; 阻塞
