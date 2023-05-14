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

;mov edi, 0x1000; 读取的目标内存
;mov ecx, 2; 起始扇区
;mov bl, 4; 扇区数量

;call read_disk

; 上面的写法在实体机上运行失败
; 使用中断加载loader, 实测可运行
mov ah, 0x02     ; 设置AH寄存器为2（读扇区）
mov al, 0x04     ; 设置AL寄存器为1（要读取的扇区数）
mov ch, 0x00     ; 设置磁头号为0
mov cl, 0x03     ; 设置扇区号为3, 从1开始编号 
mov dh, 0x00     ; 设置磁道号为0
mov dl, 0x80     ; 设置驱动器号为0x80（主引导盘, U盘）
mov bx, 0x1000   ; 设置BX寄存器为0x1000（要写入数据的内存地址）
int 0x13         ; 执行INT 0x13中断
jc read_error;


cmp word [0x1000], 0x55aa
jnz error

jmp 0:0x1008

; 阻塞
jmp $

read_disk:

    ; 设置读写扇区的数量
    mov dx, 0x1f2
    mov al, bl
    out dx, al

    inc dx; 0x1f3
    mov al, cl; 起始扇区的前八位
    out dx, al

    inc dx; 0x1f4
    shr ecx, 8
    mov al, cl; 起始扇区的中八位
    out dx, al

    inc dx; 0x1f5
    shr ecx, 8
    mov al, cl; 起始扇区的高八位
    out dx, al

    inc dx; 0x1f6
    shr ecx, 8
    and cl, 0b1111; 将高四位置为 0

    mov al, 0b1110_0000;
    or al, cl
    out dx, al; 主盘 - LBA 模式

    inc dx; 0x1f7
    mov al, 0x20; 读硬盘
    out dx, al

    xor ecx, ecx; 将 ecx 清空
    mov cl, bl; 得到读写扇区的数量

    .read:
        push cx; 保存 cx
        call .waits; 等待数据准备完毕
        call .reads; 读取一个扇区
        pop cx; 恢复 cx
        loop .read

    ret

    .waits:
        mov dx, 0x1f7
        .check:
            in al, dx
            jmp $+2; nop 直接跳转到下一行
            jmp $+2; 一点点延迟
            jmp $+2
            and al, 0b1000_1000
            cmp al, 0b0000_1000
            jnz .check
        ret

    .reads:
        mov dx, 0x1f0
        mov cx, 256; 一个扇区 256 字
        .readw:
            in ax, dx
            jmp $+2; 一点点延迟
            jmp $+2
            jmp $+2
            mov [edi], ax
            add edi, 2
            loop .readw
        ret

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
    
read_error:
    mov si, .msg
    call print
    hlt; 让CPU停止
    jmp $
    .msg db "read_error", 10, 13, 0; \r \n
    
; 填充 0
times 510 - ($ - $$) db 0

; 主引导扇区的最后两个字节必须是 0x55 0xaa
; dw 0xaa55
db 0x55, 0xaa
