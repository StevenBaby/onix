#include <onix/vm86.h>
#include <onix/syscall.h>
#include <onix/fs.h>
#include <onix/memory.h>
#include <onix/interrupt.h>
#include <onix/task.h>
#include <onix/cpu.h>
#include <onix/global.h>
#include <onix/list.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/string.h>
#include <onix/io.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 由于实模式的寻址方式，简单起见 vm86 的16进制地址后四位 必须为 0；
#define VM86_ADDR 0x10000

extern tss_t tss; // 任务状态段

enum
{
    OPCODE_INT = 0xCD,
    OPCODE_IRET = 0xCF,

    OPCODE_PUSHF = 0X9C,
    OPCODE_POPF = 0x9D,

    OPCODE_CLI = 0xFA,
    OPCODE_STI = 0xFB,

    OPCODE_IN_IMM_AL = 0xE4,
    OPCODE_IN_IMM_AX = 0xE5,
    OPCODE_IN_DX_AL = 0xEC,
    OPCODE_IN_DX_AX = 0xED,

    OPCODE_INSB = 0x6C,
    OPCODE_INSW = 0x6D,
    OPCODE_OUTSB = 0x6E,
    OPCODE_OUTSW = 0x6F,

    OPCODE_OUT_IMM_AL = 0xE6,
    OPCODE_OUT_IMM_AX = 0xE7,
    OPCODE_OUT_DX_AL = 0xEE,
    OPCODE_OUT_DX_AX = 0xEF,

    OPERAND_PREFIX = 0x66,
};

typedef struct vmbin_t
{
    u32 magic;
    u8 code[0];
} vmbin_t;

typedef struct vm86_state_t
{
    task_t *monitor; // 监控进程
    task_t *vmtask;  // 调用进程
    int vec;         // 调用号
    vm86_reg_t *reg; // 调用参数
    vm86_reg_t *out; // 输出参数
} vm86_state_t;

static vm86_state_t state_handler;
static vm86_state_t *state = &state_handler;
static vmbin_t *vmbin = (vmbin_t *)VM86_ADDR;
static task_params_t params;

static void vm86_enter()
{
    LOGK("entering vm86 mode...\n");

    tss.esp0 = (u32)state->monitor + PAGE_SIZE;

    params.addr = (u32)VM86_ADDR + PAGE_SIZE * 2;

    params.addr -= sizeof(vm86_reg_t);
    state->out = (vm86_reg_t *)params.addr;
    *state->out = *state->reg;

    params.addr -= sizeof(u16);
    *(u16 *)params.addr = state->vec;

    params.iframe = (intr_frame_t *)(tss.esp0 - sizeof(intr_frame_t));

    params.iframe->vector = 0x20;
    params.iframe->edi = 1;
    params.iframe->esi = 2;
    params.iframe->ebp = 3;
    params.iframe->esp_dummy = 4;
    params.iframe->ebx = 5;
    params.iframe->edx = 6;
    params.iframe->ecx = 7;
    params.iframe->eax = 8;

    params.iframe->gs = 0;
    params.iframe->ds = KERNEL_DATA_SELECTOR;
    params.iframe->es = KERNEL_DATA_SELECTOR;
    params.iframe->fs = KERNEL_DATA_SELECTOR;
    params.iframe->ss = VM86_ADDR >> 4;
    params.iframe->cs = VM86_ADDR >> 4;

    params.iframe->error = ONIX_MAGIC;

    params.iframe->eip = (u32)vmbin->code;
    params.iframe->eflags = EFLAG_VM;
    params.iframe->esp = params.addr & 0xffff;

    BMB;

    asm volatile(
        "movl %0, %%esp\n"
        "jmp interrupt_exit\n"
        :
        : "m"(params.iframe));
}

err_t sys_vm86(int vec, vm86_reg_t *reg)
{
    if (!state->monitor)
        return -EVM86;
    if (state->vmtask)
        return -EVM86;

    LOGK("sys vm86 int %d called...\n", vec);

    while (state->monitor->state != TASK_BLOCKED)
    {
        assert(state->monitor->state == TASK_READY);
        task_yield();
    }

    state->vmtask = running_task();
    state->vec = vec;
    state->reg = reg;

    assert(state->vmtask != state->monitor);
    assert(state->monitor->state == TASK_BLOCKED);

    task_unblock(state->monitor, EOK);

    err_t ret = task_block(state->vmtask, NULL, TASK_BLOCKED, TIMELESS);
    assert(state->vmtask == running_task());

    state->vmtask = NULL;
    state->vec = 0;
    state->reg = NULL;

    return ret;
}

static void vm86_thread()
{
    if (state->vmtask != NULL)
    {
        LOGK("unblock vmtask 0x%p\n", state->vmtask);
        unmap_zero();
        task_unblock(state->vmtask, EOK);
    }

    assert(running_task() == state->monitor);
    task_block(state->monitor, NULL, TASK_BLOCKED, TIMELESS);

    LOGK("vm86 task...\n");

    map_zero();
    vm86_enter();

    panic("VM86 process impossible routine occurred!!!\n");
}

static void gp_handler(
    u32 vector,
    u32 edi, u32 esi, u32 ebp, u32 esp_dummy,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags, u32 ss, u32 esp)
{
    if (!(eflags & EFLAG_VM))
    {
        panic("General Protection Error");
    }

    u8 *addr = (u8 *)((cs << 4) + eip);
    int oside = (addr[0] == OPERAND_PREFIX);
    u16 opcode = oside ? addr[1] : addr[0];

    intr_frame_t *frame = element_entry(intr_frame_t, eflags, &eflags);

    switch (opcode)
    {
    case OPCODE_INT:
        int nr = *(u8 *)(addr + 1);
        if (nr == 0xFE)
        {
            assert(state->vmtask);
            *state->reg = *state->out;
            vm86_thread();
        }

        frame->esp -= 6;
        *(u16 *)((frame->ss << 4) + frame->esp) = (frame->eip & 0xFFFF) + 2;
        *(u16 *)((frame->ss << 4) + frame->esp + 2) = (frame->cs & 0xFFFF);
        *(u16 *)((frame->ss << 4) + frame->esp + 4) = (frame->eflags & 0xFFFF);

        frame->cs = *(u16 *)(4 * nr + 2);
        frame->eip = *(u16 *)(4 * nr);
        break;

    case OPCODE_IRET:
        frame->eip = *(u16 *)((frame->ss << 4) + frame->esp);
        frame->cs = *(u16 *)((frame->ss << 4) + frame->esp + 2);
        frame->eflags = (frame->eflags & (~0xFFFF)) | *(u16 *)((frame->ss << 4) + frame->esp + 4);
        frame->esp += 6;
        break;
    case OPCODE_PUSHF:
        frame->esp -= 2;
        *(u16 *)((frame->ss << 4) + frame->esp) = (frame->eflags & 0xFFFF);
        frame->eip++;
        break;
    case OPCODE_POPF:
        frame->eflags = (frame->eflags & (~0xFFFF)) | *(u16 *)((frame->ss << 4) + frame->esp);
        frame->esp += 2;
        frame->eip++;
        break;
    case OPCODE_CLI:
        asm volatile("cli\n");
        frame->eip++;
        break;
    case OPCODE_STI:
        asm volatile("sti\n");
        frame->eip++;
        break;

    case OPCODE_IN_IMM_AL:
        frame->eax = (frame->eax & (~0xFF)) | inb(addr[1]);
        frame->eip += 2;
        break;
        break;
    case OPCODE_IN_IMM_AX:
        if (oside)
        {
            frame->eax = inl(addr[2]);
            frame->eip += 3;
        }
        else
        {
            frame->eax = (frame->eax & (~0xFFFF)) | inw(addr[1]);
            frame->eip += 2;
        }
        break;
    case OPCODE_IN_DX_AL:
        frame->eax = (frame->eax & (~0xFF)) | inb(frame->edx & 0xffff);
        frame->eip += 1;
        break;
    case OPCODE_IN_DX_AX:
        if (oside)
        {
            frame->eax = inl(frame->edx & 0xffff);
            frame->eip += 2;
        }
        else
        {
            frame->eax = (frame->eax & (~0xFFFF)) | inw(frame->edx & 0xffff);
            frame->eip += 1;
        }
        break;

    case OPCODE_OUT_IMM_AL:
        outb(addr[1], frame->eax & 0xff);
        frame->eip += 2;
        break;
        break;
    case OPCODE_OUT_IMM_AX:
        if (oside)
        {
            outl(addr[2], frame->eax);
            frame->eip += 3;
        }
        else
        {
            outw(addr[1], frame->eax & 0xffff);
            frame->eip += 2;
        }
        break;
    case OPCODE_OUT_DX_AL:
        outb(frame->edx & 0xffff, frame->eax & 0xff);
        frame->eip += 1;
        break;
    case OPCODE_OUT_DX_AX:
        if (oside)
        {
            outl(frame->edx & 0xffff, frame->eax);
            frame->eip += 2;
        }
        else
        {
            outw(frame->edx & 0xffff, frame->eax & 0xffff);
            frame->eip += 1;
        }
        break;
    default:
        panic("VM8086 opcode 0x%x cs:eip %x:%#x\n", opcode, cs, eip);
        break;
    }
}

void vm86_init()
{
    fd_t fd = open("/bin/vm86.bin", O_RDONLY, 0);
    if (fd < 0)
    {
        LOGK("VM86 init failure!!!\n");
        return;
    }

    int size = read(fd, (char *)vmbin, PAGE_SIZE);
    assert(size < PAGE_SIZE);
    close(fd);

    if (vmbin->magic != ONIX_MAGIC)
    {
        LOGK("VM86 magic check error!!!.\n");
        return;
    }

    state->monitor = task_create(vm86_thread, "vm86", 5, KERNEL_USER);
    state->vmtask = NULL;
    state->vec = 0;
    state->reg = NULL;

    set_exception_handler(INTR_GP, gp_handler);
}
