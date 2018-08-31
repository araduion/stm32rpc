/*
Owner = Antonescu Radu-Ion
Year = 2018

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "uart.h"
#include "spi2.h"
#include "task.h"
#include "enc28j60.h"
#include "common.h"
#include "net.h"

char cli_cmd[32], cli_cmd_ndx = 0;

void cli_test(const char *arg)
{
    uart_printf("test: %s\r\n", arg);
}

void cli_spiw(const char *arg)
{
    char rx[20];
    uint16_t len = strlen(arg) - 1;
    //write_spi2((char *)arg, strlen(arg) - 1);
    spi2_transf(rx,len,(char*)arg,len);
    uart_printf("got %x\n",rx);
}

void cli_dis_spi()
{
    *(unsigned *)0x40003800 &= ~(1 << 6);
}

void cli_sleep_test(const char *args)
{
    uint8_t i;
    uart_puts("timer test");
    for (i = 0; i < 3; i++) {
        sleep(10);
        uart_putc('.');
    }
    uart_puts("\r\n");
}

void cli_help(const char *arg);

struct cmd_list_ {
        const char cmd[20];
        void (*fn)(const char*);
    } cmd_list[] = {
        { "test", cli_test },
        { "spiw", cli_spiw },
        { "setled", cli_setled },
        { "phy", cli_read_phy_reg },
        { "creg", cli_read_ctrl_reg },
        { "cwreg", cli_write_ctrl_reg },
        { "reset", cli_reset },
        { "disspi", cli_dis_spi },
        { "sleeptest", cli_sleep_test },
        { "selftest", cli_enc28_selftest },
        { "readeth", cli_enc28_read },
        { "writeeth", cli_enc28_write },
        { "readdbg", set_read_dbg },
        { "dumpptr", cli_enc28_dump_ptr },
        { "dumppkt", cli_enc28_dump_pkts },
        { "send_pkt", cli_send_pkt },
        { "initeth", enc28j60_init },
        { "usend", cli_send_udp },
        { "help", cli_help },
    };

void cli_help(const char *arg)
{
    int i;
    uart_puts("help cmds:\r\n");
    for (i = 0; i < sizeof(cmd_list)/sizeof(struct cmd_list_); i++) {
        uart_printf("    %s\r\n", cmd_list[i].cmd);
    }
}

void cli_task(void *arg)
{
    int i;
    char *arg_pos;

    cli_cmd[sizeof(cli_cmd) - 1] = '\0';
    while (1) {
        char c = uart_getc();
        if (cli_cmd_ndx < sizeof(cli_cmd) - 1 && 8 != c && 127 != c) {
            cli_cmd[cli_cmd_ndx++] = c;
        }
        if ('\r' == c) {
            arg_pos = _strchr(cli_cmd, ' ');
            if (arg_pos) {
                *arg_pos = '\0';
                arg_pos++;
            } else {
                char *cr_pos = strchr(cli_cmd, '\r');
                *cr_pos = '\0';
            }
            cli_cmd[cli_cmd_ndx] = '\0';
            uart_puts("\r\n");
            for (i = 0; i < sizeof(cmd_list)/sizeof(struct cmd_list_); i++) {
                if (0 == strcmp(cmd_list[i].cmd, cli_cmd)) {
                    char *cr_pos = strchr(cli_cmd,'\r');
                    if (cr_pos) *cr_pos = '\0';
                    cmd_list[i].fn(arg_pos);
                    break;
                }
            }
            cli_cmd_ndx = 0;
            *cli_cmd = '\0';
        } else if (8 == c || 127 == c) { /* backspace */
            cli_cmd[--cli_cmd_ndx] = '\0';
            uart_putc(c);
        } else {
            uart_putc(c);
        }
        task_next();
    }
}

