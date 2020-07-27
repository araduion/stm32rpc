/*
Owner = Antonescu Radu-Ion
Year = 2016

< THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __TIMER_H__
#define __TIMER_H__

#include "task.h"

#ifndef CPU_FREQ
#error CPU_FREQ
#endif

#define PCLOCK CPU_FREQ

typedef struct task_sleep_ {
    unsigned long wake_time;
    uint8_t task_id;
    bool in_use;
    struct task_sleep_ *next;
} task_sleep_t;

void sys_tick_handler(void);
void timer_init(void);
void timer_pause(void);
int in_timer_list(uint8_t id);
unsigned long cr_time(void);
void sleep(unsigned long s);

#endif

