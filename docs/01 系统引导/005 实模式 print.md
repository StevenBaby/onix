# 实模式 print

- ah: 0x0e
- al: 字符
- int 0x10

    xchg bx, bx; bochs 魔术断点

```s
mov si, booting
call print

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
```