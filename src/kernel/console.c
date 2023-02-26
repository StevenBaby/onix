#include <onix/io.h>
#include <onix/string.h>
#include <onix/interrupt.h>
#include <onix/device.h>

#define CRT_ADDR_REG 0x3D4 // CRT(6845)索引寄存器
#define CRT_DATA_REG 0x3D5 // CRT(6845)数据寄存器

#define CRT_START_ADDR_H 0xC // 显示内存起始位置 - 高位
#define CRT_START_ADDR_L 0xD // 显示内存起始位置 - 低位
#define CRT_CURSOR_H 0xE     // 光标位置 - 高位
#define CRT_CURSOR_L 0xF     // 光标位置 - 低位

#define MEM_BASE 0xB8000              // 显卡内存起始位置
#define MEM_SIZE 0x4000               // 显卡内存大小
#define MEM_END (MEM_BASE + MEM_SIZE) // 显卡内存结束位置
#define WIDTH 80                      // 屏幕文本列数
#define HEIGHT 25                     // 屏幕文本行数
#define ROW_SIZE (WIDTH * 2)          // 每行字节数
#define SCR_SIZE (ROW_SIZE * HEIGHT)  // 屏幕字节数

#define NUL 0x00
#define ENQ 0x05
#define ESC 0x1B // ESC
#define BEL 0x07 // \a
#define BS 0x08  // \b
#define HT 0x09  // \t
#define LF 0x0A  // \n
#define VT 0x0B  // \v
#define FF 0x0C  // \f
#define CR 0x0D  // \r
#define DEL 0x7F

enum ST
{
    ST_NORMAL = 0,
    ST_BOLD = 1,
    ST_BLINK = 5,
    ST_REVERSE = 7,
};

#define STYLE 7
#define BLINK 0x80
#define BOLD 0x0F
#define UNDER 0X0F

enum COLOR
{
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    YELLOW = 6,
    WHITE = 7,
};

#define ERASE 0x0720
#define ARG_NR 16

enum state
{
    STATE_NOR,
    STATE_ESC,
    STATE_QUE,
    STATE_ARG,
    STATE_CSI,
};

typedef struct console_t
{
    u32 mem_base; // 内存基地址
    u32 mem_size; // 内存大小
    u32 mem_end;  // 内存结束位置

    u32 screen;   // 当前屏幕位置
    u32 scr_size; // 屏幕内存大小

    union
    {
        u32 pos;   // 当前光标位置
        char *ptr; // 位置指针
    };
    u32 x;        // 光标坐标 x
    u32 y;        // 光标坐标 y
    u32 saved_x;  // 保存的 x
    u32 saved_y;  // 保存的 y
    u32 width;    // 屏幕宽度
    u32 height;   // 屏幕高度
    u32 row_size; // 行内存大小

    u8 state;         // 当前状态
    u32 args[ARG_NR]; // 参数
    u32 argc;         // 参数数量
    u32 ques;         //

    u16 erase; // 清屏字符
    u8 style;  // 当前样式
} console_t;

static console_t console;

// 设置当前显示器开始的位置
static _inline void set_screen(console_t *con)
{
    outb(CRT_ADDR_REG, CRT_START_ADDR_H); // 开始位置高地址
    outb(CRT_DATA_REG, ((con->screen - con->mem_base) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_START_ADDR_L); // 开始位置低地址
    outb(CRT_DATA_REG, ((con->screen - con->mem_base) >> 1) & 0xff);
}

// 设置当前显示器光标位置
static _inline void set_cursor(console_t *con)
{
    outb(CRT_ADDR_REG, CRT_CURSOR_H); // 光标高地址
    outb(CRT_DATA_REG, ((con->pos - con->mem_base) >> 9) & 0xff);
    outb(CRT_ADDR_REG, CRT_CURSOR_L); // 光标低地址
    outb(CRT_DATA_REG, ((con->pos - con->mem_base) >> 1) & 0xff);
}

// 设置光标位置
static _inline void set_xy(console_t *con, u32 x, u32 y)
{
    if (x > con->width || y >= con->height)
        return;
    con->x = x;
    con->y = y;
    con->pos = con->screen + y * con->row_size + (x << 1);
    // set_cursor(con);
}

// 保存光标位置
static _inline void save_cursor(console_t *con)
{
    con->saved_x = con->x;
    con->saved_y = con->y;
}

// 清除屏幕
static _inline void erase_screen(console_t *con, u16 *start, u32 count)
{
    int nr = 0;
    while (nr++ < count)
    {
        *start++ = con->erase;
    }
}

// 清空控制台
void console_clear(console_t *con)
{
    con->screen = con->mem_base;
    con->pos = con->mem_base;
    con->x = con->y = 0;

    set_cursor(con);
    set_screen(con);
    erase_screen(con, (u16 *)con->mem_base, con->mem_size >> 1);
}

// 向上滚屏
static void scroll_up(console_t *con)
{
    if ((con->screen + con->scr_size + con->row_size) >= con->mem_end)
    {
        memcpy((void *)con->mem_base, (void *)con->screen, con->scr_size);
        con->pos -= (con->screen - con->mem_base);
        con->screen = con->mem_base;
    }

    u16 *ptr = (u16 *)(con->screen + con->scr_size);
    erase_screen(con, ptr, con->width);

    con->screen += con->row_size;
    con->pos += con->row_size;
    set_screen(con);
}

// 向下滚屏
static void scroll_down(console_t *con)
{
    con->screen -= con->row_size;
    if (con->screen < con->mem_base)
    {
        con->screen = con->mem_base;
    }
    set_screen(con);
}

// 换行 \n
static _inline void lf(console_t *con)
{
    if (con->y + 1 < con->height)
    {
        con->y++;
        con->pos += con->row_size;
        return;
    }
    scroll_up(con);
}

// 光标回到开始位置
static _inline void cr(console_t *con)
{
    con->pos -= (con->x << 1);
    con->x = 0;
}

// TAB
static _inline void tab(console_t *con)
{
    int offset = 8 - (con->x & 7);
    con->x += offset;
    con->pos += offset << 1;
    if (con->x >= con->width)
    {
        con->x -= con->width;
        con->pos -= con->row_size;
        lf(con);
    }
}

// 退格
static _inline void bs(console_t *con)
{
    if (!con->x)
        return;
    con->x--;
    con->pos -= 2;
    *(u16 *)con->pos = con->erase;
}

// 删除当前字符
static _inline void del(console_t *con)
{
    *(u16 *)con->pos = con->erase;
}

// 输出字符
static _inline void chr(console_t *con, char ch)
{
    if (con->x >= con->width)
    {
        con->x -= con->width;
        con->pos -= con->row_size;
        lf(con);
    }

    *con->ptr++ = ch;
    *con->ptr++ = con->style;
    con->x++;
}

extern void start_beep();

// 正常状态
static _inline void state_normal(console_t *con, char ch)
{
    switch (ch)
    {
    case NUL:
        break;
    case BEL:
        start_beep();
        break;
    case BS:
        bs(con);
        break;
    case HT:
        tab(con);
        break;
    case LF:
        lf(con);
        cr(con);
        break;
    case VT:
    case FF:
        lf(con);
        break;
    case CR:
        cr(con);
        break;
    case DEL:
        del(con);
        break;
    case ESC:
        con->state = STATE_ESC;
        break;
    default:
        chr(con, ch);
        break;
    }
}

// esc 状态
static _inline void state_esc(console_t *con, char ch)
{
    switch (ch)
    {
    case '[':
        con->state = STATE_QUE;
        break;
    case 'E':
        lf(con);
        cr(con);
        break;
    case 'M':
        // go up
        break;
    case 'D':
        lf(con);
        break;
    case 'Z':
        // response
        break;
    case '7':
        save_cursor(con);
        break;
    case '8':
        set_xy(con, con->saved_x, con->saved_y);
        break;
    default:
        break;
    }
}

// 参数状态
static _inline bool state_arg(console_t *con, char ch)
{
    if (con->argc >= ARG_NR)
        return false;
    if (ch == ';')
    {
        con->argc++;
        return false;
    }
    if (ch >= '0' && ch <= '9')
    {
        con->args[con->argc] = con->args[con->argc] * 10 + ch - '0';
        return false;
    }
    con->argc++;
    con->state = STATE_CSI;
    return true;
}

// 清屏
static _inline void csi_J(console_t *con)
{
    int count = 0;
    int start = 0;
    switch (con->args[0])
    {
    case 0: // 擦除屏幕中光标后面的部分
        count = (con->screen + con->scr_size - con->pos) >> 1;
        start = con->pos;
        break;
    case 1: // 擦除屏幕中光标前面的部分
        count = (con->pos - con->screen) >> 1;
        start = con->screen;
        break;
    case 2: // 整个屏幕上的字符
        count = con->scr_size >> 1;
        start = con->screen;
        break;
    default:
        return;
    }

    erase_screen(con, (u16 *)start, count);
}

// 删除行
static _inline void csi_K(console_t *con)
{
    int count = 0;
    int start = 0;
    switch (con->args[0])
    {
    case 0: // 删除行光标后
        count = con->width - con->x;
        start = con->pos;
        break;
    case 1: // 删除行光标前
        count = con->x;
        start = con->pos - (con->x << 1);
        break;
    case 2: // 删除整行
        count = con->width;
        start = con->pos - (con->x << 1);
        break;
    default:
        return;
    }

    erase_screen(con, (u16 *)start, count);
}

// 插入一行
static _inline void insert_line(console_t *con)
{
    u16 *start = (u16 *)(con->screen + con->y * con->row_size);
    for (size_t i = 2; true; i++)
    {
        void *src = (void *)(con->mem_end - (i * con->row_size));
        if (src < (void *)start)
            break;

        memcpy(src + con->row_size, src, con->row_size);
    }
    erase_screen(con, (u16 *)(con->screen + (con->y) * con->row_size), con->width);
}

// 插入多行
static _inline void csi_L(console_t *con)
{
    int nr = con->args[0];
    if (nr > con->height)
        nr = con->height;
    else if (!nr)
        nr = 1;
    while (nr--)
    {
        insert_line(con);
    }
}

// 删除一行
static _inline void delete_line(console_t *con)
{
    u16 *start = (u16 *)(con->screen + con->y * con->row_size);
    for (size_t i = 1; true; i++)
    {
        void *src = start + (i * con->row_size);
        if (src >= (void *)con->mem_end)
            break;

        memcpy(src - con->row_size, src, con->row_size);
    }
    erase_screen(con, (u16 *)(con->mem_end - con->row_size), con->width);
}

// 删除多行
static _inline void csi_M(console_t *con)
{
    int nr = con->args[0];
    if (nr > con->height)
        nr = con->height;
    else if (!nr)
        nr = 1;
    while (nr--)
    {
        delete_line(con);
    }
}

// 删除当前字符
static _inline void delete_char(console_t *con)
{
    u16 *ptr = (u16 *)con->ptr;
    u16 i = con->x;
    while (++i < con->width)
    {
        *ptr = *(ptr + 1);
        ptr++;
    }
    *ptr = con->erase;
}

// 删除多个字符
static _inline void csi_P(console_t *con)
{
    int nr = con->args[0];
    if (nr > con->height)
        nr = con->height;
    else if (!nr)
        nr = 1;
    while (nr--)
    {
        delete_char(con);
    }
}

// 插入字符
static _inline void insert_char(console_t *con)
{
    u16 *ptr = (u16 *)con->ptr + (con->width - con->x - 1);
    while (ptr > (u16 *)con->ptr)
    {
        *ptr = *(ptr - 1);
        ptr--;
    }
    *con->ptr = con->erase;
}

// 插入多个字符
static _inline void csi_at(console_t *con)
{
    int nr = con->args[0];
    if (nr > con->height)
        nr = con->height;
    else if (!nr)
        nr = 1;
    while (nr--)
    {
        insert_char(con);
    }
}

// 修改样式
static _inline void csi_m(console_t *con)
{
    con->style = 0;
    for (size_t i = 0; i < con->argc; i++)
    {
        if (con->args[i] == ST_NORMAL)
            con->style = STYLE;

        else if (con->args[i] == ST_BOLD)
            con->style = BOLD;

        else if (con->args[i] == BLINK)
            con->style |= BLINK;

        else if (con->args[i] == ST_REVERSE)
            con->style = (con->style >> 4) | (con->style << 4);

        else if (con->args[i] >= 30 && con->args[i] <= 37)
            con->style = con->style & 0xF8 | (con->args[i] - 30);

        else if (con->args[i] >= 40 && con->args[i] <= 47)
            con->style = con->style & 0x8F | ((con->args[i] - 40) << 4);
    }
    con->erase = (con->style << 8) | 0x20;
}

// CSI 状态
static _inline void state_csi(console_t *con, char ch)
{
    con->state = STATE_NOR;
    switch (ch)
    {
    case 'G':
    case '`':
        if (con->args[0])
            con->args[0]--;
        set_xy(con, con->args[0], con->y);
        break;
    case 'A': // 光标上移一行或 n 行
        if (!con->args[0])
            con->args[0]++;
        set_xy(con, con->x, con->y - con->args[0]);
        break;
    case 'B':
    case 'e': // 光标下移一行或 n 行
        if (!con->args[0])
            con->args[0]++;
        set_xy(con, con->x, con->y + con->args[0]);
        break;
    case 'C':
    case 'a': // 光标右移一列或 n 列
        if (!con->args[0])
            con->args[0]++;
        set_xy(con, con->x + con->args[0], con->y);
        break;
    case 'D': // 光标左移一列或 n 列
        if (!con->args[0])
            con->args[0]++;
        set_xy(con, con->x - con->args[0], con->y);
        break;
    case 'E': // 光标下移一行或 n 行，并回到 0 列
        if (!con->args[0])
            con->args[0]++;
        set_xy(con, 0, con->y + con->args[0]);
        break;
    case 'F': // 光标上移一行或 n 行，并回到 0 列
        if (!con->args[0])
            con->args[0]++;
        set_xy(con, 0, con->y - con->args[0]);
        break;
    case 'd': // 设置行号
        if (con->args[0])
            con->args[0]--;
        set_xy(con, con->x, con->args[0]);
        break;
    case 'H': // 设置行号和列号
    case 'f':
        if (con->args[0])
            con->args[0]--;
        if (con->args[1])
            con->args[1]--;
        set_xy(con, con->args[1], con->args[0]);
        break;
    case 'J': // 清屏
        csi_J(con);
        break;
    case 'K': // 行删除
        csi_K(con);
        break;
    case 'L': // 插入行
        csi_L(con);
        break;
    case 'M': // 删除行
        csi_M(con);
        break;
    case 'P': // 删除字符
        csi_P(con);
        break;
    case '@': // 插入字符
        csi_at(con);
        break;
    case 'm': // 修改样式
        csi_m(con);
        break;
    case 'r': // 设置起始行号和终止行号
        break;
    case 's':
        save_cursor(con);
    case 'u':
        set_xy(con, con->saved_x, con->saved_y);
    default:
        break;
    }
}

// 写控制台
int console_write(console_t *con, char *buf, u32 count)
{
    bool intr = interrupt_disable(); // 禁止中断

    // console_t *con = &console;
    char ch;
    int nr = 0;
    while (nr++ < count)
    {
        ch = *buf++;
        switch (con->state)
        {
        case STATE_NOR:
            state_normal(con, ch);
            break;
        case STATE_ESC:
            state_esc(con, ch);
            break;
        case STATE_QUE:
            memset(con->args, 0, sizeof(con->args));
            con->argc = 0;
            con->state = STATE_ARG;
            con->ques = (ch == '?');
            if (con->ques)
                break;
        case STATE_ARG:
            if (!state_arg(con, ch))
                break;
        case STATE_CSI:
            state_csi(con, ch);
            break;
        default:
            break;
        }
    }
    set_cursor(con);
    // 恢复中断
    set_interrupt_state(intr);
    return nr;
}

void console_init()
{
    console_t *con = &console;
    con->mem_base = MEM_BASE;
    con->mem_size = (MEM_SIZE / ROW_SIZE) * ROW_SIZE;
    con->mem_end = con->mem_base + con->mem_size;
    con->width = WIDTH;
    con->height = HEIGHT;
    con->row_size = con->width * 2;
    con->scr_size = con->width * con->height * 2;

    con->erase = ERASE;
    con->style = STYLE;
    console_clear(con);

    device_install(
        DEV_CHAR, DEV_CONSOLE,
        con, "console", 0,
        NULL, NULL, console_write);
}