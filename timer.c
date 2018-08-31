/*
Owner = Antonescu Radu-Ion
Year = 2016

< THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "timer.h"
#include "st/stm32f10x.h"
#include "arm/irq.h"
#include "task.h"

task_sleep_t sleep_list[] = { [0 ... TASK_NUM] .in_use = false }, *first_sleep = NULL;
static unsigned long time_counter = 0;

void timer_resume(void);

void sys_tick_handler(void)	__attribute__((interrupt("IRQ")));

void sys_tick_handler(void) /* Timer 2 */
{
    TIM2->SR = 0;
    time_counter++;
    while (first_sleep && first_sleep->wake_time <= time_counter) {
        task_resume(first_sleep->task_id);
        first_sleep->in_use = false;
        first_sleep->task_id = 0;
        first_sleep = first_sleep->next;
    }
    timer_resume();
}

unsigned long cr_time(void)
{
    return time_counter;
}

static inline task_sleep_t* new_sleep(void)
{
    int i;
    for (i = 0; i < TASK_NUM; i++) {
        if (false == sleep_list[i].in_use) {
            sleep_list[i].next = NULL;
            sleep_list[i].in_use = true;
            return &sleep_list[i];
        }
    }
    return NULL;
}

/* granularity 1/10s */
void sleep(unsigned long s)
{
    int wake_time = time_counter + s;
    task_sleep_t *aux, *prev;
    uint8_t cr_task_id;

    cr_task_id = cr_task->task_id;
    if (NULL == first_sleep || false == first_sleep->in_use) {
        aux = new_sleep();
        first_sleep = aux;
    } else {
        if (first_sleep->wake_time >= wake_time) {
            /* insert first */
            aux = new_sleep();
            aux->next = first_sleep;
            first_sleep = aux;
        } else {
            /* find the right place for it */
            for (prev = first_sleep, aux = first_sleep->next;
                 aux != NULL && aux->wake_time < wake_time;
                 prev = aux, aux = aux->next) {
                /* do stuff... nothing :p */
            }
            if (NULL == aux) {
                /* append cr. entry to the list */
                aux = new_sleep();
                prev->next = aux;
            } else {
                /* insert between prev and aux */
                task_sleep_t *node = new_sleep();
                prev->next = node;
                node->next = aux;
                aux = node; /* data is filled on aux */
            }
        }
    }
    aux->task_id = cr_task_id;
    aux->wake_time = wake_time;
    task_suspend(cr_task_id);

    task_next();
}

void timer_init(void)
{
    int i;
    
    /* pg. 362 */
    TIM2->PSC = 0xFFFF;
    uart_printf("timer using pclock %09d Hz\r\n", PCLOCK);
    TIM2->ARR = (PCLOCK/1000000);
    TIM2->DIER |= 1<<6|1;
    NVIC_enableIRQ(TIM2_IRQn);
    TIM2->CR1 = 1|1<<7|1<<3;
}

void timer_pause(void)
{
    TIM2->CR1 &= ~1;
}

void timer_resume(void)
{
    TIM2->CR1 |= 1;
}

int in_timer_list(uint8_t id)
{
    task_sleep_t *it;
    for (it = first_sleep; it && it->in_use; it = it->next) {
        if (it->task_id == id && it->in_use) {
            return  it->wake_time - time_counter;
        }
    }
    return 0;
}

