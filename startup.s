.syntax unified
.section .text
.global pend_sv_handler
.global main

.section .vectors

reset:
.word start_sp
.word _start+1       @ start point
.word default_handler   @ NMI
.word _hard_fault    @ hard-fault
.word _mem_manage    @ mem-manage
.word _buf_fault     @ bus-fault
.word _usage_fault   @ usage-fault
.word 0
.word 0
.word 0
.word 0
.word default_handler   @ SVC-handler
.word default_handler   @ debug-monitor
.word 0
.word pend_sv_handler+1 @ pend-SV
.word sys_tick_handler  @ sys-tick
.word default_handler   @ watchdog
.word default_handler   @ PVD
.word default_handler   @ TAMPer
.word default_handler   @ RTC Wkup
.word default_handler   @ Flash
.word default_handler   @ RCC
.word default_handler   @ EINT0
.word default_handler   @ EINT1
.word default_handler   @ EINT2
.word default_handler   @ EINT3
.word default_handler   @ EINT4
.word dma_ch_handler    @ DMA0
.word dma_ch_handler    @ DMA1
.word dma_ch_handler    @ DMA2
.word dma_ch_handler    @ DMA3
.word dma_ch_handler    @ DMA4
.word dma_ch_handler    @ DMA5
.word dma_ch_handler    @ DMA6
.word dma_ch_handler    @ DMA7.word default_handler   @ ADC
.word default_handler   @ CAN1 TX
.word default_handler   @ CAN1 RX0
.word default_handler   @ CAN1 RX1
.word default_handler   @ CAN1 SCE
.word default_handler   @ EXTI 9-5
.word default_handler   @ TIM1 BREAK TIM9
.word default_handler   @ TIM1 UP TIM10
.word default_handler   @ TIM1 TRG COM TIM11
.word default_handler   @ TIM1 CC
.word sys_tick_handler  @ TIM2
.word default_handler   @ TIM3
.word default_handler   @ TIM4
.word default_handler   @ I2C1 EV
.word default_handler   @ I2C1 ER
.word default_handler   @ I2C2 EV
.word default_handler   @ I2C2 ER
.word default_handler   @ SPI1
.word spi2_int_handler  @ SPI2
.word uart_handler      @ uart 1
.word uart_handler      @ uart 2
.word uart_handler      @ uart 3
.word default_handler   @ EXTI 15
.word default_handler   @ RTC Alarm
.word default_handler   @ OTG FS WKUP * 42

.text

_start:
    ldr sp, =start_sp

    ldr r2, =bss_size
    cmp r2, #0
    beq copy_data
    mov r0, #0
    ldr r1, =sbss

setzero:
    str.w r0, [r1], #4
    subs r2, r2, #4
    bne setzero

copy_data:
    ldr r0, =flash_data
    ldr r1, =ram_sdata
    ldr r2, =data_size
copy_data_loop:
    ldr.w r4, [r0], #4
    str.w r4, [r1], #4
    subs r2, r2, #4
    bne copy_data_loop

    b main    

pend_sv_handler:
    nop
    ldr r0, =first_run
    ldr r1, [r0]

    @ we need it in both cases
    ldr r0, =cr_task
    ldr r3, [r0]     @ get ptr to current struct

    @ we don't save context for MSP; all threads run with PSP
    cmp r1, #1
    beq next_task_handler

    @ save context
    mrs r2, psp
    stmdb r2!, {r4-r11}
    str r2, [r3]     @ set current SP @ store curent SP

next_task_handler:
    ldr r0, =first_run
    mov r1, #0
    str r1, [r0]

    ldr r0, =cr_task
    add r3, #4
    ldr r2, [r3]     @ get next task
    str r2, [r0]     @ store next task to cr_task
    ldr r1, [r2]     @ get sp of next task

@ Restore context
    ldmfd r1!, {r4-r11}
    msr psp, r1

ret_psp:
    @ set PSP/MSP when return as current SP
    orr lr, lr, #4 @ return to thread mode using PSP
    bx lr

