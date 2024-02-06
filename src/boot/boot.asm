[org 0x7c00]

; 设置屏幕模式为文本模式，清除屏幕
mov ax, 3
int 0x10

; 初始化段寄存器
mov ax, 0
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

mov si, booting
call print

; 读磁盘
mov ah, 0x42
mov dl, 0x80
mov si, dap
int 0x13

; 检查读磁盘错误
cmp ah, 0
jnz error

; 检查 loader 魔数
cmp word [0x1000], 0x55aa
jnz error

jmp 0:0x1008

; 阻塞
jmp $

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

booting:
    db "Booting Onix...", 10, 13, 0; \n\r

error:
    mov si, .msg
    call print
    hlt; 让 CPU 停止
    jmp $
    .msg db "Booting Error!!!", 10, 13, 0

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

; 填充 0
times 510 - ($ - $$) db 0

; 主引导扇区的最后两个字节必须是 0x55 0xaa
; dw 0xaa55
db 0x55, 0xaa
