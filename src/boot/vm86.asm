[bits 16]

VM86_ADDR equ 0x10000

[org 0x0000]
    dd 0x20220205; magic

    ; 修改 int 指令的操作数为传输的参数
    mov ax, (VM86_ADDR >> 4)
    mov ds, ax
    pop ax
    mov byte [int_num_ptr], al

    ; 向寄存器写入传入的值
    pop ax
    mov es, ax

    pop ax
    mov ds, ax

    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax

    ; 执行 int num 指令
    ; int 指令的操作码
    db 0xCD; int
int_num_ptr:
    db 0x00; ; int 指令的操作数

    ; 向栈中写入输出的寄存器值
    push ax;
    push bx;
    push cx;
    push dx;
    push si;
    push di;
    push bp;

    mov ax, ds;
    push ax

    mov ax, es;
    push ax

    ; 修改 int 指令的操作数为 0
    mov ax, (VM86_ADDR >> 4)
    mov ds, ax
    mov byte [int_num_ptr], 0

    int 0xfe
