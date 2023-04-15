#include <onix/cpu.h>

// 检测是否支持 cpuid 指令
bool cpu_check_cpuid()
{
    bool ret;
    asm volatile(
        "pushfl \n" // 保存 eflags

        "pushfl \n"                   // 得到 eflags
        "xorl $0x00200000, (%%esp)\n" // 反转 ID 位
        "popfl\n"                     // 写入 eflags

        "pushfl\n"                  // 得到 eflags
        "popl %%eax\n"              // 写入 eax
        "xorl (%%esp), %%eax\n"     // 将写入的值与原值比较
        "andl $0x00200000, %%eax\n" // 得到 ID 位
        "shrl $21, %%eax\n"         // 右移 21 位，得到是否支持

        "popfl\n" // 恢复 eflags
        : "=a"(ret));
    return ret;
}

// 得到供应商 ID 字符串
void cpu_vendor_id(cpu_vendor_t *item)
{
    asm volatile(
        "cpuid \n"
        : "=a"(*((u32 *)item + 0)),
          "=b"(*((u32 *)item + 1)),
          "=d"(*((u32 *)item + 2)),
          "=c"(*((u32 *)item + 3))
        : "a"(0));
    item->info[12] = 0;
}

void cpu_version(cpu_version_t *item)
{
    asm volatile(
        "cpuid \n"
        : "=a"(*((u32 *)item + 0)),
          "=b"(*((u32 *)item + 1)),
          "=c"(*((u32 *)item + 2)),
          "=d"(*((u32 *)item + 3))
        : "a"(1));
}
