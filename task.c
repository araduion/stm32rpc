#include "task.h"
#include "uart.h"

unsigned char cr_stack[TASK_NUM][STACK_SIZE] __attribute__ ((aligned(4)));
task_t *ts_start = NULL, *ts_end = NULL;
task_t *cr_task = NULL;
bool pend_op_q = false;
task_op_t task_ops[TASK_NUM];
task_t tasks[TASK_NUM];

void task_destroy(void);

void task_create(void (*entry_point)(void *arg), void *arg_value)
{
    hw_stack_frame_t *pframe;
    tasks[task_index].stack = cr_stack[task_index + 1];
    pframe = (hw_stack_frame_t *)&tasks[task_index].stack[ - sizeof(hw_stack_frame_t)];
    tasks[task_index].sp = pframe;
    pframe->r0 = (unsigned int)arg_value;
    pframe->r1 = 0;
    pframe->r2 = 0;
    pframe->r3 = 0;
    pframe->r12 = 0;
    pframe->pc = (unsigned int)entry_point - 1;
    pframe->lr = (unsigned int)task_destroy;
    pframe->psr = 0x21000000;
    tasks[task_index].sp -= sizeof(sw_stack_frame_t);
    if (0 == task_index) {
        cr_task = &tasks[0];
        tasks[0].next = &tasks[0];
        tasks[0].prev = &tasks[0];
    } else {
        tasks[task_index].prev = &tasks[task_index - 1];
        tasks[task_index - 1].next = &tasks[task_index];
        tasks[task_index].next = &tasks[0];
        cr_task = &tasks[task_index];
        tasks[0].prev = cr_task;
    }
    tasks[task_index].task_id = task_index;
    task_index++;
    uart_printf("%s: task_num %d >= task_index %d\r\n", __func__, TASK_NUM, task_index);
}

void task_next(void)
{
    *((uint32_t volatile *)0xE000ED04) = 1 << 28; // trigger PendSV
}

void task_destroy(void)
{
    uart_printf("task exit!\r\n");
}

/* thread 0 content */
void check_pend_op(void)
{
    int i;
    if (pend_op_q) {
        pend_op_q = false;
        for (i = 1; i < TASK_NUM; i++) {
            task_op_t *cr_op = &task_ops[i];
            if (cr_op->op[0] != 0) {
                cr_op->op[0](NULL,i);
                cr_op->op[0] = 0;
            }
            if (cr_op->op[1] != 0) {
                cr_op->op[1](NULL,i);
                cr_op->op[1] = 0;
            }
        }
    }
    task_next();
}

/* DO NOT CALL THIS FROM INTERRUPT!!!
 * interrupt should resume tasks when data from peripheral is available 
 * false means not yet; scheduled*/
bool _task_suspend(const char * caller, uint8_t id)
{
    task_t *next, *prev;

    if (id == 0) {
        uart_printf("tried to suspend task 0 from %d!\r\n", cr_task->task_id);
        return false;
    }

    if (caller != NULL) tasks[id].last_op_caller = caller;

    if (TASK_SUSPENDED == tasks[id].flags && task_ops[id].op[1]) {
        /* task was set as 'to be resumed' but task suspend occur before resuming */
        task_ops[id].op[1] = NULL;
        return false;
    }
    if (id == cr_task->task_id) {
        /* cannot remove thread from ourself because next will point in suspended tasks */
        task_ops[id].op[0] = _task_suspend;
        pend_op_q = true;
        return false;
    }
    next = tasks[id].next;
    prev = tasks[id].prev;
    if (NULL == ts_start) {
        ts_start = &tasks[id];
        ts_end = ts_start;
        ts_start->prev = NULL;
    } else {
        ts_end->next = &tasks[id];
        tasks[id].prev = ts_end;
        if (ts_end) ts_end = ts_end->next;
    }
    if (ts_end) ts_end->next = NULL;
    prev->next = next;
    next->prev = prev;
    tasks[id].flags = TASK_SUSPENDED;
    return true;
}

/* false means not yet; scheduled */
bool _task_resume(const char * caller, uint8_t id)
{
    register task_t *prev, *next;

    if (caller != NULL) tasks[id].last_op_caller = caller;

    if (TASK_RUNNING == tasks[id].flags &&
        task_ops[id].op[0] != 0) { /* not yet suspended ... add resume callback*/
        task_ops[id].op[1] = _task_resume;
        pend_op_q = true;
        return false;
    }
    if ((*(uint32_t volatile *)0xE000ED04 & 511) != 0
             && TASK_SUSPENDED == tasks[id].flags) {
        /* do not modify task linked list from interrupt
         * because is not reentrant safe; just schedule it */
        task_ops[id].op[1] = _task_resume;
        pend_op_q = true;
        return false;
    }

    /* remove from suspended tasks */
    prev = tasks[id].prev;
    next = tasks[id].next;
    if (prev != NULL) {
        prev->next = next;
    } else {
        ts_start = ts_start->next;
    }
    if (next != NULL) {
        next->prev = prev;
    } else {
        ts_end = ts_end->prev;
        if (ts_end) ts_end->next = NULL;
    }
    /* insert process in queue */
    next = cr_task->next;
    cr_task->next = &tasks[id];
    tasks[id].next = next;
    next->prev = &tasks[id];
    tasks[id].prev = cr_task;
    tasks[id].flags = TASK_RUNNING;
    return true;
}

