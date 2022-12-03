#include <onix/types.h>
#include <onix/syscall.h>
#include <onix/fs.h>
#include <onix/memory.h>
#include <onix/string.h>
#include <onix/stdlib.h>
#include <onix/assert.h>
#include <onix/debug.h>
#include <onix/task.h>
#include <onix/string.h>
#include <onix/arena.h>

#if 0
#include <elf.h>
#endif

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

typedef u32 Elf32_Word;
typedef u32 Elf32_Addr;
typedef u32 Elf32_Off;
typedef u16 Elf32_Half;

// ELF 文件标记
typedef struct ELFIdent
{
    u8 ei_magic[4];    // 内容为 0x7F, E, L, F
    u8 ei_class;       // 文件种类 1-32位，2-64 位
    u8 ei_data;        // 标记大小端 1-小端，2-大端
    u8 ei_version;     // 与 e_version 一样，必须为 1
    u8 ei_pad[16 - 7]; // 占满 16 个字节
} _packed ELFIdent;

// ELF 文件头
typedef struct Elf32_Ehdr
{
    ELFIdent e_ident;       // ELF 文件标记，文件最开始的 16 个字节
    Elf32_Half e_type;      // 文件类型，见 Etype
    Elf32_Half e_machine;   // 处理器架构类型，标记运行的 CPU，见 EMachine
    Elf32_Word e_version;   // 文件版本，见 EVersion
    Elf32_Addr e_entry;     // 程序入口地址
    Elf32_Off e_phoff;      // program header offset 程序头表在文件中的偏移量
    Elf32_Off e_shoff;      // section header offset 节头表在文件中的偏移量
    Elf32_Word e_flags;     // 处理器特殊标记
    Elf32_Half e_ehsize;    // ELF header size ELF 文件头大小
    Elf32_Half e_phentsize; // program header entry size 程序头表入口大小
    Elf32_Half e_phnum;     // program header number 程序头数量
    Elf32_Half e_shentsize; // section header entry size 节头表入口大小
    Elf32_Half e_shnum;     // section header number 节头表数量
    Elf32_Half e_shstrndx;  // 节字符串表在节头表中的索引
} Elf32_Ehdr;

// ELF 文件类型
enum Etype
{
    ET_NONE = 0,        // 无类型
    ET_REL = 1,         // 可重定位文件
    ET_EXEC = 2,        // 可执行文件
    ET_DYN = 3,         // 动态链接库
    ET_CORE = 4,        // core 文件，未说明，占位
    ET_LOPROC = 0xff00, // 处理器相关低值
    ET_HIPROC = 0xffff, // 处理器相关高值
};

// ELF 机器(CPU)类型
enum EMachine
{
    EM_NONE = 0,  // No machine
    EM_M32 = 1,   // AT&T WE 32100
    EM_SPARC = 2, // SPARC
    EM_386 = 3,   // Intel 80386
    EM_68K = 4,   // Motorola 68000
    EM_88K = 5,   // Motorola 88000
    EM_860 = 7,   // Intel 80860
    EM_MIPS = 8,  // MIPS RS3000
};

// ELF 文件版本
enum EVersion
{
    EV_NONE = 0,    // 不可用版本
    EV_CURRENT = 1, // 当前版本
};

// 程序头
typedef struct Elf32_Phdr
{
    Elf32_Word p_type;   // 段类型，见 SegmentType
    Elf32_Off p_offset;  // 段在文件中的偏移量
    Elf32_Addr p_vaddr;  // 加载到内存中的虚拟地址
    Elf32_Addr p_paddr;  // 加载到内存中的物理地址
    Elf32_Word p_filesz; // 文件中占用的字节数
    Elf32_Word p_memsz;  // 内存中占用的字节数
    Elf32_Word p_flags;  // 段标记，见 SegmentFlag
    Elf32_Word p_align;  // 段对齐约束
} Elf32_Phdr;

// 段类型
enum SegmentType
{
    PT_NULL = 0,    // 未使用
    PT_LOAD = 1,    // 可加载程序段
    PT_DYNAMIC = 2, // 动态加载信息
    PT_INTERP = 3,  // 动态加载器名称
    PT_NOTE = 4,    // 一些辅助信息
    PT_SHLIB = 5,   // 保留
    PT_PHDR = 6,    // 程序头表
    PT_LOPROC = 0x70000000,
    PT_HIPROC = 0x7fffffff,
};

enum SegmentFlag
{
    PF_X = 0x1, // 可执行
    PF_W = 0x2, // 可写
    PF_R = 0x4, // 可读
};

typedef struct Elf32_Shdr
{
    Elf32_Word sh_name;      // 节名，在字符串表中的索引
    Elf32_Word sh_type;      // 节类型，见 SectionType
    Elf32_Word sh_flags;     // 节标记，见 SectionFlag
    Elf32_Addr sh_addr;      // 节地址
    Elf32_Off sh_offset;     // 节在文件中的偏移量
    Elf32_Word sh_size;      // 节大小
    Elf32_Word sh_link;      // 保存了头表索引链接，与节类型相关
    Elf32_Word sh_info;      // 额外信息，与节类型相关
    Elf32_Word sh_addralign; // 地址对齐约束
    Elf32_Word sh_entsize;   // 子项入口大小
} Elf32_Shdr;

enum SectionType
{
    SHT_NULL = 0,            // 不可用
    SHT_PROGBITS = 1,        // 程序信息
    SHT_SYMTAB = 2,          // 符号表
    SHT_STRTAB = 3,          // 字符串表
    SHT_RELA = 4,            // 有附加重定位
    SHT_HASH = 5,            // 符号哈希表
    SHT_DYNAMIC = 6,         // 动态链接信息
    SHT_NOTE = 7,            // 标记文件信息
    SHT_NOBITS = 8,          // 该节文件中无内容
    SHT_REL = 9,             // 无附加重定位
    SHT_SHLIB = 10,          // 保留，用于非特定的语义
    SHT_DYNSYM = 11,         // 符号表
    SHT_LOPROC = 0x70000000, // 以下与处理器相关
    SHT_HIPROC = 0x7fffffff,
    SHT_LOUSER = 0x80000000,
    SHT_HIUSER = 0xffffffff,
};

enum SectionFlag
{
    SHF_WRITE = 0x1,           // 执行时可写
    SHF_ALLOC = 0x2,           // 执行时占用内存，有些节执行时可以不在内存中
    SHF_EXECINSTR = 0x4,       // 包含可执行的机器指令，节里有代码
    SHF_MASKPROC = 0xf0000000, // 保留，与处理器相关
};

typedef struct Elf32_Sym
{
    Elf32_Word st_name;  // 符号名称，在字符串表中的索引
    Elf32_Addr st_value; // 符号值，与具体符号相关
    Elf32_Word st_size;  // 符号的大小
    u8 st_info;          // 指定符号类型和约束属性，见 SymbolBinding
    u8 st_other;         // 为 0，无意义
    Elf32_Half st_shndx; // 符号对应的节表索引
} Elf32_Sym;

// 通过 info 获取约束
#define ELF32_ST_BIND(i) ((i) >> 4)
// 通过 info 获取类型
#define ELF32_ST_TYPE(i) ((i)&0xF)
// 通过 约束 b 和 类型 t 获取 info
#define ELF32_ST_INFO(b, t) (((b) << 4) + ((t)&0xf))

// 符号约束
enum SymbolBinding
{
    STB_LOCAL = 0,   // 外部不可见符号，优先级最高
    STB_GLOBAL = 1,  // 外部可见符号
    STB_WEAK = 2,    // 弱符号，外部可见，如果符号重定义，则优先级更低
    STB_LOPROC = 13, // 处理器相关低位
    STB_HIPROC = 15, // 处理器相关高位
};

// 符号类型
enum SymbolType
{
    STT_NOTYPE = 0,  // 未定义
    STT_OBJECT = 1,  // 数据对象，比如 变量，数组等
    STT_FUNC = 2,    // 函数或可执行代码
    STT_SECTION = 3, // 节，用于重定位
    STT_FILE = 4,    // 文件，节索引为 SHN_ABS，约束为 STB_LOCAL，
                     // 而且优先级高于其他 STB_LOCAL 符号
    STT_LOPROC = 13, // 处理器相关
    STT_HIPROC = 15, // 处理器相关
};

// 检查 ELF 文件头
static bool elf_validate(Elf32_Ehdr *ehdr)
{
    // 不是 ELF 文件
    if (memcmp(&ehdr->e_ident, "\177ELF\1\1\1", 7))
        return false;

    // 不是可执行文件
    if (ehdr->e_type != ET_EXEC)
        return false;

    // 不是 386 程序
    if (ehdr->e_machine != EM_386)
        return false;

    // 版本不可识别
    if (ehdr->e_version != EV_CURRENT)
        return false;

    if (ehdr->e_phentsize != sizeof(Elf32_Phdr))
        return false;

    return true;
}

static void load_segment(inode_t *inode, Elf32_Phdr *phdr)
{
    assert(phdr->p_align == 0x1000);      // 对齐到页
    assert((phdr->p_vaddr & 0xfff) == 0); // 对齐到页

    u32 vaddr = phdr->p_vaddr;

    // 需要页的数量
    u32 count = div_round_up(MAX(phdr->p_memsz, phdr->p_filesz), PAGE_SIZE);

    for (size_t i = 0; i < count; i++)
    {
        u32 addr = vaddr + i * PAGE_SIZE;
        assert(addr >= USER_EXEC_ADDR && addr < USER_MMAP_ADDR);
        link_page(addr);
    }

    inode_read(inode, (char *)vaddr, phdr->p_filesz, phdr->p_offset);
    if (phdr->p_filesz < phdr->p_memsz)
    {
        memset((char *)vaddr + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);
    }

    // 如果段不可写，则置为只读
    if ((phdr->p_flags & PF_W) == 0)
    {
        for (size_t i = 0; i < count; i++)
        {
            u32 addr = vaddr + i * PAGE_SIZE;
            page_entry_t *entry = get_entry(addr, false);
            entry->write = false;
            entry->readonly = true;
            flush_tlb(addr);
        }
    }

    task_t *task = running_task();
    if (phdr->p_flags == (PF_R | PF_X))
    {
        task->text = vaddr;
    }
    else if (phdr->p_flags == (PF_R | PF_W))
    {
        task->data = vaddr;
    }

    task->end = MAX(task->end, (vaddr + count * PAGE_SIZE));
}

static u32 load_elf(inode_t *inode)
{
    link_page(USER_EXEC_ADDR);

    int n = 0;
    // 读取 ELF 文件头
    n = inode_read(inode, (char *)USER_EXEC_ADDR, sizeof(Elf32_Ehdr), 0);
    assert(n == sizeof(Elf32_Ehdr));

    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)USER_EXEC_ADDR;
    if (!elf_validate(ehdr))
        return EOF;

    // 读取程序段头表
    Elf32_Phdr *phdr = (Elf32_Phdr *)(USER_EXEC_ADDR + sizeof(Elf32_Ehdr));
    n = inode_read(inode, (char *)phdr, ehdr->e_phnum * ehdr->e_phentsize, ehdr->e_phoff);

    Elf32_Phdr *ptr = phdr;
    for (size_t i = 0; i < ehdr->e_phnum; i++)
    {
        if (ptr->p_type != PT_LOAD)
            continue;
        load_segment(inode, ptr);
        ptr++;
    }

    return ehdr->e_entry;
}

// 计算参数数量
static int count_argv(char *argv[])
{
    if (!argv)
        return 0;
    int i = 0;
    while (argv[i])
        i++;
    return i;
}

static u32 copy_argv_envp(char *filename, char *argv[], char *envp[])
{
    // 计算参数数量
    int argc = count_argv(argv) + 1;
    int envc = count_argv(envp);

    // 分配内核内存，用于临时存储参数
    u32 pages = alloc_kpage(4);
    u32 pages_end = pages + 4 * PAGE_SIZE;

    // 内核临时栈顶地址
    char *ktop = (char *)pages_end;
    // 用户栈顶地址
    char *utop = (char *)USER_STACK_TOP;

    // 内核参数
    char **argvk = (char **)alloc_kpage(1);
    // 以 NULL 结尾
    argvk[argc] = NULL;

    // 内核环境变量
    char **envpk = argvk + argc + 1;
    // 以 NULL 结尾
    envpk[envc] = NULL;

    int len = 0;
    // 拷贝 envp
    for (int i = envc - 1; i >= 0; i--)
    {
        // 计算长度
        len = strlen(envp[i]) + 1;
        // 得到拷贝地址
        ktop -= len;
        utop -= len;
        // 拷贝字符串到内核
        memcpy(ktop, envp[i], len);
        // 数组中存储的是用户态栈地址
        envpk[i] = utop;
    }

    // 拷贝 argv
    for (int i = argc - 1; i > 0; i--)
    {
        // 计算长度
        len = strlen(argv[i - 1]) + 1;
        
        // 得到拷贝地址
        ktop -= len;
        utop -= len;
        // 拷贝字符串到内核
        memcpy(ktop, argv[i - 1], len);
        // 数组中存储的是用户态栈地址
        argvk[i] = utop;
    }

    // 拷贝 argv[0]，程序路径
    len = strlen(filename) + 1;
    ktop -= len;
    utop -= len;
    memcpy(ktop, filename, len);
    argvk[0] = utop;

    // 将 envp 数组拷贝内核
    ktop -= (envc + 1) * 4;
    memcpy(ktop, envpk, (envc + 1) * 4);

    // 将 argv 数组拷贝内核
    ktop -= (argc + 1) * 4;
    memcpy(ktop, argvk, (argc + 1) * 4);

    // 为 argc 赋值
    ktop -= 4;
    *(int *)ktop = argc;

    assert((u32)ktop > pages);

    // 将参数和环境变量拷贝到用户栈
    len = (pages_end - (u32)ktop);
    utop = (char *)(USER_STACK_TOP - len);
    memcpy(utop, ktop, len);

    // 释放内核内存
    free_kpage((u32)argvk, 1);
    free_kpage(pages, 4);

    return (u32)utop;
}

extern int sys_brk();

int sys_execve(char *filename, char *argv[], char *envp[])
{
    inode_t *inode = namei(filename);
    int ret = EOF;
    if (!inode)
        goto rollback;

    // 不是常规文件
    if (!ISFILE(inode->desc->mode))
        goto rollback;

    // 文件不可执行
    if (!permission(inode, P_EXEC))
        goto rollback;

    task_t *task = running_task();
    strncpy(task->name, filename, TASK_NAME_LEN);

    // 处理参数和环境变量
    u32 top = copy_argv_envp(filename, argv, envp);

    // 首先释放原程序的堆内存
    task->end = USER_EXEC_ADDR;
    sys_brk(USER_EXEC_ADDR);

    // 加载程序
    u32 entry = load_elf(inode);
    if (entry == EOF)
        goto rollback;

    // 设置堆内存地址
    sys_brk((u32)task->end);

    iput(task->iexec);
    task->iexec = inode;

    intr_frame_t *iframe = (intr_frame_t *)((u32)task + PAGE_SIZE - sizeof(intr_frame_t));

    iframe->edx = 0; // TODO 动态链接器的地址
    iframe->eip = entry;
    iframe->esp = top;

    // ROP 技术，直接从中断返回
    // 通过 eip 跳转到 entry 执行
    asm volatile(
        "movl %0, %%esp\n"
        "jmp interrupt_exit\n" ::"m"(iframe));

rollback:
    iput(inode);
    return ret;
}
