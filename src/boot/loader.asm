[org 0x1000]

dd 0x55aa; 魔数，用于判断错误

kernel_size: dd KERNEL_SIZE; 内核大小

; 打印字符串
mov si, loading
call print

; xchg bx, bx; 断点

detect_memory:
    ; 将 ebx 置为 0
    xor ebx, ebx

    ; es:di 结构体的缓存位置
    mov ax, 0
    mov es, ax
    mov edi, ards_buffer

    mov edx, 0x534d4150; 固定签名

.next:
    ; 子功能号
    mov eax, 0xe820
    ; ards 结构的大小 (字节)
    mov ecx, 20
    ; 调用 0x15 系统调用
    int 0x15

    ; 如果 CF 置位，表示出错
    jc error

    ; 将缓存指针指向下一个结构体
    add di, cx

    ; 将结构体数量加一
    inc dword [ards_count]

    cmp ebx, 0
    jnz .next

    mov si, detecting
    call print

; 读取内核
read_kernel:

    sub esp, 4 * 3; 三个变量
    mov dword [esp], 0; 读出的数量
    mov dword [esp + 4], 10         ; ecx 初始扇区位置
    mov dword [esp + 8], LOADERADDR ; edi 目标内存位置
    BLOCK_SIZE equ 100              ; 一次读取的扇区数量

; 读取一块
.read_block: 

    mov ax, [esp + 8]    ; 读取的目标内存便宜
    mov [dap.offset], ax

    mov ax, [esp + 10]  ; 读取的目标内存段
    shl ax, 12          ; 向左移位 (segment << 4 + offset)
    mov [dap.segment], ax

    mov eax, [esp + 4]     ; 起始扇区
    mov dword [dap.lbal], eax

    mov ax, BLOCK_SIZE  ; 扇区数量
    mov [dap.sectors], ax

    call read_disk

    add dword [esp + 8], BLOCK_SIZE * 512  ; edi 目标内存位置
    add dword [esp + 4], BLOCK_SIZE        ; ecx 初始扇区位置

    mov eax, [kernel_size]
    add dword [esp], BLOCK_SIZE * 512
    cmp dword [esp], eax; 判断已读数量与 kernel_size 的大小

    jl .read_block

; 准备保护模式
prepare_protected_mode:
    ; xchg bx, bx; 断点

    cli; 关闭中断

    ; 打开 A20 线
    in al,  0x92
    or al, 0b10
    out 0x92, al

    lgdt [gdt_ptr]; 加载 gdt

    ; 启动保护模式
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; 用跳转来刷新缓存，启用保护模式
    jmp dword code_selector:protect_mode

print:
    mov ah, 0x0e
.next:
    mov al, [si]
    cmp al, 0
    jz .done
    int 0x10
    inc si
    jmp .next
.done:
    ret

read_disk:
    ; 读磁盘
    mov ah, 0x42
    mov dl, 0x80
    mov si, dap
    int 0x13

    ; 检查读磁盘错误
    cmp ah, 0
    jnz error
    ret

loading:
    db "Loading Onix...", 10, 13, 0; \n\r
detecting:
    db "Detecting Memory Success...", 10, 13, 0; \n\r

error:
    mov si, .msg
    call print
    hlt; 让 CPU 停止
    jmp $
    .msg db "Loading Error!!!", 10, 13, 0

    align 4         ; 四字节对齐
dap:                ; Disk Address Packet
    .size db 0x10   ; DAP 大小 16 字节
    .unused db 0x00 ; 保留
    .sectors dw 4   ; 扇区数量

    ; 因为是小端，所以 offset:segment
    .offset dw 0x1000   ; 缓存偏移
    .segment dw 0x00    ; 缓存段
    .lbal dd 0x02       ; lba 低 32 位
    .lbah dd 0x00       ; lba 高 16 位

[bits 32]
protect_mode:
    ; xchg bx, bx; 断点
    mov ax, data_selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax; 初始化段寄存器

    mov esp, 0x10000; 修改栈顶

    mov eax, 0x20220205; 内核魔数
    mov ebx, ards_count; ards 数量指针

    jmp dword code_selector:ENTRYPOINT

    ud2; 表示出错

code_selector equ (1 << 3)
data_selector equ (2 << 3)

memory_base equ 0; 内存开始的位置：基地址

; 内存界限 4G / 4K - 1
memory_limit equ ((1024 * 1024 * 1024 * 4) / (1024 * 4)) - 1

gdt_ptr:
    dw (gdt_end - gdt_base) - 1
    dd gdt_base
gdt_base:
    dd 0, 0; NULL 描述符
gdt_code:
    dw memory_limit & 0xffff; 段界限 0 ~ 15 位
    dw memory_base & 0xffff; 基地址 0 ~ 15 位
    db (memory_base >> 16) & 0xff; 基地址 16 ~ 23 位
    ; 存在 - dlp 0 - S _ 代码 - 非依从 - 可读 - 没有被访问过
    db 0b_1_00_1_1_0_1_0;
    ; 4k - 32 位 - 不是 64 位 - 段界限 16 ~ 19
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf;
    db (memory_base >> 24) & 0xff; 基地址 24 ~ 31 位
gdt_data:
    dw memory_limit & 0xffff; 段界限 0 ~ 15 位
    dw memory_base & 0xffff; 基地址 0 ~ 15 位
    db (memory_base >> 16) & 0xff; 基地址 16 ~ 23 位
    ; 存在 - dlp 0 - S _ 数据 - 向上 - 可写 - 没有被访问过
    db 0b_1_00_1_0_0_1_0;
    ; 4k - 32 位 - 不是 64 位 - 段界限 16 ~ 19
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf;
    db (memory_base >> 24) & 0xff; 基地址 24 ~ 31 位
gdt_end:

ards_count:
    dd 0
ards_buffer:

