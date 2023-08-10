#include <onix/types.h>
#include <onix/mouse.h>
#include <onix/memory.h>
#include <onix/io.h>
#include <onix/interrupt.h>
#include <onix/string.h>
#include <onix/arena.h>
#include <onix/device.h>
#include <onix/task.h>
#include <onix/mutex.h>
#include <onix/printk.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define PS2_DATA 0x60
#define PS2_CTRL 0X64
#define PS2_STATUS 0X64

#define MOUSE_WRITE 0xD4

#define PS2_PORT1_IRQ 0x01
#define PS2_PORT2_IRQ 0x02
#define PS2_PORT1_TLATE 0x40

#define PS2_DISABLE_PORT2 0xA7
#define PS2_ENABLE_PORT2 0xA8
#define PS2_DISABLE_PORT1 0xAD
#define PS2_ENABLE_PORT1 0xAE

#define PS2_READ_CONFIG 0x20
#define PS2_WRITE_CONFIG 0x60

#define WAIT_INPUT 0
#define WAIT_OUTPUT 1

#define MOUSE_DEFAULT 0
#define MOUSE_SCROLLWHEEL 1
#define MOUSE_BUTTONS 2

enum
{
    CMD_RESET = 0xFF,
    CMD_RESEND = 0xFE,
    CMD_SET_DEFAULT = 0xF6,
    CMD_DISABLE_PACKET_STREAMING = 0xF5,
    CMD_ENABLE_PACKET_STREAMING = 0xF4,
    CMD_SET_SAMPLE_RATE = 0xF3,
    CMD_GET_MOUSEID = 0xF2,
    CMD_SET_REMOTE_MODE = 0xF0,
    CMD_SET_WRAP_MODE = 0xEE,
    CMD_RESET_WRAP_MODE = 0xEC,
    CMD_SET_STREAM_MODE = 0xEA,
    CMD_REQUEST_SINGLE_PACKET = 0xEB,
    CMD_STATUS_REQUEST = 0xE9,
    CMD_SET_RESOLUTION = 0xE8,
};

enum
{
    MOUSE_BIT_LEFT = 0x01,
    MOUSE_BIT_RIGHT = 0x02,
    MOUSE_BIT_MIDDLE = 0x04,
    MOUSE_BIT_ALWAYS1 = 0x08,
    MOUSE_BIT_X_SIGN = 0x10,
    MOUSE_BIT_Y_SIGN = 0x20,
    MOUSE_BIT_X_OVERFLOW = 0x40,
    MOUSE_BIT_Y_OVERFLOW = 0x80,
};

#define MOUSE_PACKETS 64

typedef struct mouse_t
{
    int mode;
    u8 cycle;
    u8 byte[4];
    lock_t lock;
    mpacket_t packets[MOUSE_PACKETS];
    int head;
    int tail;
    task_t *waiter;
} mouse_t;

static mouse_t mouse;

static err_t ps2_wait_input()
{
    int timeout = 100000;
    while (timeout--)
    {
        if (!(inb(PS2_CTRL) & 0b10))
            return EOK;
    }
    return -ETIME;
}

static err_t ps2_wait_output()
{
    int timeout = 100000;
    while (timeout--)
    {
        if ((inb(PS2_CTRL) & 1))
            return EOK;
    }
    return -ETIME;
}

static void ps2_command(u8 cmd)
{
    ps2_wait_input();
    outb(PS2_CTRL, cmd);
}

static u8 ps2_command_response(u8 cmd)
{
    ps2_wait_input();
    outb(PS2_CTRL, cmd);
    ps2_wait_output();
    return inb(PS2_DATA);
}

static void ps2_command_arg(u8 cmd, u8 arg)
{
    ps2_wait_input();
    outb(PS2_CTRL, cmd);
    ps2_wait_input();
    outb(PS2_DATA, arg);
}

static u8 ps2_mouse_write(u8 write)
{
    ps2_command_arg(MOUSE_WRITE, write);
    ps2_wait_output();
    return inb(PS2_DATA);
}

static u8 ps2_read()
{
    ps2_wait_output();
    return inb(PS2_DATA);
}

static void finish_packet()
{
    mouse.cycle = 0;

    int ctrl = mouse.byte[0];
    int x = mouse.byte[1];
    int y = mouse.byte[2];

    if (x && (ctrl & MOUSE_BIT_X_SIGN))
        x -= 0x100;
    if (y && (ctrl & MOUSE_BIT_Y_SIGN))
        y -= 0x100;
    if (ctrl & MOUSE_BIT_X_OVERFLOW || ctrl & MOUSE_BIT_Y_OVERFLOW)
    {
        x = 0;
        y = 0;
    }

    mpacket_t *pkt = &mouse.packets[mouse.head];

    mouse.head = (mouse.head + 1) % MOUSE_PACKETS;
    if (mouse.head == mouse.tail)
        mouse.tail = (mouse.tail + 1) % MOUSE_PACKETS;

    memset(pkt, 0, sizeof(mpacket_t));

    pkt->x = x;
    pkt->y = y;
    pkt->buttons = ctrl & (MOUSE_BIT_LEFT | MOUSE_BIT_RIGHT | MOUSE_BIT_MIDDLE);
    if (mouse.mode == MOUSE_SCROLLWHEEL && mouse.byte[3])
    {
        if ((int8)mouse.byte[3] > 0)
        {
            pkt->buttons |= MOUSE_SCROLL_DOWN;
        }
        else if ((int8)mouse.byte[3] < 0)
        {
            pkt->buttons |= MOUSE_SCROLL_UP;
        }
    }

    if (mouse.waiter)
    {
        task_unblock(mouse.waiter, EOK);
        mouse.waiter = NULL;
    }

    // LOGK("mouse move (%d, %d) buttons %d\n", x, y, pkt->buttons);
}

int mouse_read(void *dev, char *buf, u32 count)
{
    if (count < sizeof(mpacket_t))
        return -EINVAL;
    int ret = EOF;
    lock_acquire(&mouse.lock);

    if (mouse.head == mouse.tail)
    {
        goto rollback;
        // mouse.waiter = running_task();
        // ret = task_block(mouse.waiter, NULL, TASK_WAITING, TIMELESS);
        // if (ret < EOK)
        //     goto rollback;
    }

    assert(mouse.head != mouse.tail);
    mpacket_t *pkt = &mouse.packets[mouse.tail];
    mouse.tail = (mouse.tail + 1) % MOUSE_PACKETS;
    memcpy(buf, pkt, sizeof(mpacket_t));
    ret = sizeof(mpacket_t);

rollback:
    lock_release(&mouse.lock);
    return ret;
}

static void mouse_handler(int vector)
{
    u8 byte = inb(PS2_DATA);
    switch (mouse.cycle)
    {
    case 0:
        mouse.byte[0] = byte;
        if (!(byte & MOUSE_BIT_ALWAYS1))
            break;
        mouse.cycle++;
        break;
    case 1:
        mouse.byte[1] = byte;
        mouse.cycle++;
        break;
    case 2:
        mouse.byte[2] = byte;
        if (mouse.mode == MOUSE_SCROLLWHEEL || mouse.mode == MOUSE_BUTTONS)
        {
            mouse.cycle++;
            break;
        }
        finish_packet();
        break;
    case 3:
        mouse.byte[3] = byte;
        finish_packet();
        break;
    default:
        break;
    }

    send_eoi(vector);
}

void mouse_init()
{
    LOGK("initial mouse...\n");

    memset(&mouse, 0, sizeof(mouse_t));
    mouse.mode = MOUSE_DEFAULT;
    lock_init(&mouse.lock);

    ps2_command(PS2_DISABLE_PORT1);
    ps2_command(PS2_DISABLE_PORT2);

    // 清空缓存
    size_t timeout = 1024;
    while ((inb(PS2_STATUS) & 1) && timeout > 0)
    {
        timeout--;
        inb(PS2_DATA);
    }
    if (timeout == 0)
        return;

    u8 status = ps2_command_response(PS2_READ_CONFIG);
    status |= (PS2_PORT1_IRQ | PS2_PORT2_IRQ | PS2_PORT1_TLATE);
    ps2_command_arg(PS2_WRITE_CONFIG, status);

    ps2_command(PS2_ENABLE_PORT1);
    ps2_command(PS2_ENABLE_PORT2);

    ps2_mouse_write(CMD_SET_DEFAULT);
    ps2_mouse_write(CMD_ENABLE_PACKET_STREAMING);

    ps2_mouse_write(CMD_GET_MOUSEID);
    ps2_read();
    ps2_mouse_write(CMD_SET_SAMPLE_RATE);
    ps2_mouse_write(200);
    ps2_mouse_write(CMD_SET_SAMPLE_RATE);
    ps2_mouse_write(100);
    ps2_mouse_write(CMD_SET_SAMPLE_RATE);
    ps2_mouse_write(80);
    ps2_mouse_write(CMD_GET_MOUSEID);
    u8 result = ps2_read();
    if (result == 3)
        mouse.mode = MOUSE_SCROLLWHEEL;

    set_interrupt_handler(IRQ_MOUSE, mouse_handler);
    set_interrupt_mask(IRQ_MOUSE, true);
    set_interrupt_mask(IRQ_CASCADE, true);

    device_install(
        DEV_CHAR, DEV_MOUSE,
        NULL, "mouse", 0,
        NULL, mouse_read, NULL);
}
