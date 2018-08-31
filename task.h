#ifndef __TASK_H__
#define __TASK_H__

#include "common.h"

#define TASK_NUM 3
#define STACK_SIZE 1024

extern unsigned char cr_stack[TASK_NUM][STACK_SIZE];

typedef enum {
    TASK_RUNNING = 0,
    TASK_SUSPENDED,
} task_state_t;

typedef union task_op_ {
    bool (*op[2])(const char *caller,uint8_t id); /* op[0] = suspend; op[1] = resume */
} task_op_t;

extern task_op_t task_ops[TASK_NUM];

//This defines the stack frame that is saved  by the hardware
typedef struct hw_stack_frame_ {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t psr;
} hw_stack_frame_t;

//This defines the stack frame that must be saved by the software
typedef struct sw_stack_frame_ {
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
} sw_stack_frame_t;

typedef struct context_{
    sw_stack_frame_t sw;
    hw_stack_frame_t hw;
} context_t;

//Circular queue
typedef struct task_ {
    void *sp;
    struct task_ *next;
    struct task_ *prev;
    unsigned char *stack;
    uint8_t task_id;
    task_state_t flags;
    const char *last_op_caller;
} task_t;

extern task_t *cr_task;
extern int task_index;

extern task_t tasks[TASK_NUM];

void task_create(void (*entry_point)(void *arg), void *);
void task_next(void);
void check_pend_op(void);
bool _task_suspend(const char * caller, uint8_t id);

#define task_suspend(id) _task_suspend(__func__,id)

bool _task_resume(const char * caller, uint8_t id);

#define task_resume(id) _task_resume(__func__,id)

#define GET_HW_CONTEXT(task_id) ((*(context_t*)(tasks[task_id].sp)).hw)
#define GET_SW_CONTEXT(task_id) ((*(context_t*)(tasks[task_id].sp)).sw)

#endif
