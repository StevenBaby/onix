#include <onix/task.h>
#include <onix/timer.h>

extern int sys_kill();

static void task_alarm(timer_t *timer)
{
    timer->task->alarm = NULL;
    sys_kill(timer->task->pid, SIGALRM);
}

int sys_alarm(int sec)
{
    task_t *task = running_task();
    if (task->alarm)
    {
        timer_put(task->alarm);
    }
    task->alarm = timer_add(sec * 1000, task_alarm, 0);
}