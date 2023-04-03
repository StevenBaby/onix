# ACPI

ACPI是高级配置电源接口，利用他我们可以实现关机，重启，响应关机按钮等操作

ACPI表由BIOS创建，并存放在内存中，OS需要一个入口去获取到所有的表。
入口由BIOS放在某个固定的位置(对于Legacy BIOS和UEFI，OS获取的方式不同)，这个入口被称为RSDP，Root System Description Pointer。它是一个结构体，其结构如下：
提取其中的RsdtAddress即可获取地址

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

FACP和SSDT是平级的关系,而DSDT的指针在FACP里面
但是SSDT和DSDT里面数据的格式是完全一样的 (当然只是数据格式,如果数据也完全一样那么我们看一个就够了)
偏移0~36 依然是这个Header,我们只需要其中的length,signature
偏移36~? 数据
我们要做的就是在"数据"这一部分里查找关机的命令
经过查找,第一行是标识符,第二行才是真正的数据

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

目前只实现了关机，后续会增加重启等其他操作。

# 参考

https://forum.osdev.org/viewtopic.php?t=16990
