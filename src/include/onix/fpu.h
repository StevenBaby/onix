#ifndef ONIX_FPU_H
#define ONIX_FPU_H

#include <onix/types.h>
#include <onix/task.h>

enum
{
    CR0_PE = 1 << 0,  // Protection Enable 启用保护模式
    CR0_MP = 1 << 1,  // Monitor Coprocessor
    CR0_EM = 1 << 2,  // Emulation 启用模拟，表示没有 FPU
    CR0_TS = 1 << 3,  // Task Switch 任务切换，延迟保存浮点环境
    CR0_ET = 1 << 4,  // Extension Type 保留
    CR0_NE = 1 << 5,  // Numeric Error 启用内部浮点错误报告
    CR0_WP = 1 << 16, // Write Protect 写保护（禁止超级用户写入只读页）帮助写时复制
    CR0_AM = 1 << 18, // Alignment Mask 对齐掩码
    CR0_NW = 1 << 29, // Not Write-Through 不是直写
    CR0_CD = 1 << 30, // Cache Disable 禁用内存缓冲
    CR0_PG = 1 << 31, // Paging 启用分页
};

// Intel® 64 and IA-32 Architectures Software Developer's Manual
// Figure 8-9. Protected Mode x87 FPU State Image in Memory, 32-Bit Format

typedef struct fpu_t
{
    u16 control;
    u16 RESERVED;
    u16 status;
    u16 RESERVED;
    u16 tag;
    u16 RESERVED;
    u32 fip0;
    u32 fop0;
    u32 fdp0;
    u32 fdp1;
    u8 regs[80];
} _packed fpu_t;

bool fpu_check();
void fpu_disable(task_t *task);
void fpu_enable(task_t *task);

#endif