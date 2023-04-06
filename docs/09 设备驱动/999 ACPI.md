# ACPI

ACPI [^acpi] 是高级配置电源接口，利用他我们可以实现关机，重启，响应关机按钮等操作。相关代码来自 [^forum]。

ACPI 表由 BIOS 创建，并存放在内存中，OS 需要一个入口去获取到所有的表。

入口由 BIOS 放在某个固定的位置(对于 Legacy BIOS 和 UEFI，OS 获取的方式不同)，这个入口被称为 RSDP，Root System Description Pointer。它是一个结构体，提取其中的 RsdtAddress 即可获取地址，其结构如下：

```c++
struct RSDPtr
{
    u8 Signature[8];
    u8 CheckSum;
    u8 OemID[6];
    u8 Revision;
    u32 *RsdtAddress;
    ...
};
```

```c++
    return (unsigned int *)rsdp->RsdtAddress;
```

# 提取关机数据

FACP 和 SSDT 是平级的关系，而 DSDT 的指针在 FACP 里面，但是 SSDT 和 DSDT 里面数据的格式是完全一样的 (当然只是数据格式，如果数据也完全一样那么我们看一个就够了)

偏移 0~36 依然是这个 Header，我们只需要其中的 length,signature，偏移 36~? 数据我们要做的就是在 "数据 " 这一部分里查找关机的命令，经过查找，第一行是标识符，第二行才是真正的数据：

```c++
struct FACP
{
    u8 Signature[4];
    u32 Length;
    u8 unneded1[40 - 8];
    u32 *DSDT;
    u8 unneded2[48 - 44];
    u32 *SMI_CMD;
    u8 ACPI_ENABLE;
    u8 ACPI_DISABLE;
    u8 unneded3[64 - 54];
    u32 *PM1a_CNT_BLK;
    u32 *PM1b_CNT_BLK;
    u8 unneded4[89 - 72];
    u8 PM1_CNT_LEN;
    ...
};
```

重启则比较简单，目前我知道的有两种方式：

1. 向键盘控制器(端口0x64)发送reset指令(0xfe)。

2. 向acpi的reset寄存器发送FADT表中的ResetValue。

## 参考

[^forum]: <https://forum.osdev.org/viewtopic.php?t=16990>
[^acpi]: <https://wiki.osdev.org/Acpi>
