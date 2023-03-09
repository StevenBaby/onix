#include <onix/timer.h>
#include <onix/interrupt.h>
#include <onix/syscall.h>
#include <onix/task.h>
#include <onix/mutex.h>
#include <onix/arena.h>
#include <onix/errno.h>
#include <onix/assert.h>
#include <onix/debug.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern u32 volatile jiffies;
extern u32 jiffy;

static list_t timer_list;

static timer_t *timer_get()
{
    timer_t *timer = (timer_t *)kmalloc(sizeof(timer_t));
    return timer;
}

void timer_put(timer_t *timer)
{
    list_remove(&timer->node);
    kfree(timer);
}

void default_timeout(timer_t *timer)
{
    assert(timer->task->node.next);
    task_unblock(timer->task, -ETIME);
}

timer_t *timer_add(u32 expire_ms, handler_t handler, void *arg)
{
    timer_t *timer = timer_get();
    timer->task = running_task();
    timer->expires = jiffies + expire_ms / jiffy;
    timer->handler = handler;
    timer->arg = arg;
    timer->active = false;

    list_insert_sort(&timer_list, &timer->node, element_node_offset(timer_t, node, expires));

    return timer;
}

u32 timer_expires()
{
    if (list_empty(&timer_list))
    {
        return EOF;
    }
    timer_t *timer = element_entry(timer_t, node, timer_list.head.next);
    return timer->expires;
}

void timer_init()
{
    LOGK("timer init...\n");
    list_init(&timer_list);
}

// 从定时器链表中找到 task 任务的定时器，删除之，用于 task_exit
void timer_remove(task_t *task)
{
    list_t *list = &timer_list;
    for (list_node_t *ptr = list->head.next; ptr != &list->tail;)
    {
        timer_t *timer = element_entry(timer_t, node, ptr);
        ptr = ptr->next;
        if (timer->task != task)
            continue;
        timer_put(timer);
    }
}

void timer_wakeup()
{
    while (timer_expires() <= jiffies)
    {
        timer_t *timer = element_entry(timer_t, node, timer_list.head.next);
        timer->active = true;

        assert(timer->expires <= jiffies);
        if (timer->handler)
        {
            timer->handler(timer);
        }
        else
        {
            default_timeout(timer);
        }
        timer_put(timer);
    }
}