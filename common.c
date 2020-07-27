/*
Owner = Antonescu Radu-Ion
Year = 2016

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "common.h"

static int get_num_digits(int i, int base)
{
    int ndigit = 0;
    while (i > 0) {
        i /= base;
        ndigit++;
    }
    return ndigit;
}

void itoa(int i, char *buf, int base)
{
    int pos, num_digits = get_num_digits(i, base);
    if (num_digits == 0) {
        strcpy(buf, "0");
        return;
    }
    pos = num_digits - 1;
    while (pos >= 0) {
        buf[pos] = (i % base);
        if (buf[pos] < 10) {
            buf[pos] += '0';
        } else {
            buf[pos] += 'a' - 10;
        }
        pos--;
        i = i / base;
    }
    buf[num_digits] = '\0';
}

int pow_int(char exp, int base)
{
    int i, res = 1;
    for (i = 0 ; i < exp; i++)
        res *= base;
    return res;
}

int atoi(const char *str)
{
    char c, i, slen = strlen(str) - 1;
    int res = 0;
    for (i = 0; i <= slen; i++) {
        c = str[slen - i];
        res += pow_int(i, 10)*(c - '0');
    }
    return res;
}

int atoi16(const char *str)
{
    char c, i, slen = strlen(str) - 1;
    int res = 0;
    for (i = 0; i <= slen; i++) {
        c = str[slen - i];
        if (c >= '0' && c <= '9') {
            res += pow_int(i, 16)*(c - '0');
        } else {
            if (c < 'a') c += 'A' - 'a';
            res += pow_int(i, 16)*(c - 'a' + 10);
        }
    }
    return res;
}

char* strchr(char *str, int ch)
{
    char *str_it;
    for (str_it = str; *str_it != '\0'; str_it++) {
        if (*str_it == ch) return str_it;
    }
    return NULL;
}

int strcmp(const char *str1, const char *str2)
{
    while (*str1 && *str2) {
        if (*str1 < *str2) {
            return -1;
        } else if (*str1 > *str2) {
            return 1;
        }
        str1++; str2++;
    }
    if ('\0' == *str1 && '\0' == *str2) {
        return 0;
    }
    if (*str1) return 1;
    return -1;
}

int strncmp(const char *str1, const char *str2, size_t len)
{
    while (*str1 && *str2 && len > 0) {
        if (*str1 < *str2) {
            return -1;
        } else if (*str1 > *str2) {
            return 1;
        }
        str1++; str2++; len--;
    }
    if (0 == len) return 0;
    if ('\0' == *str1 && '\0' == *str2) {
        return 0;
    }
    if (*str1) return 1;
    return -1;
}

int strlen(const char *str)
{
    int len = 0;
    while(str[len] != '\0') len++;
    return len;
}

char *strcpy (char *dest, const char *src)
{
    while (*src != '\0')
        *dest++ = *src++;
    *dest = '\0';
}

char *_strchr(const char *s, int c)
{
    char *it;
    for (it = (char*)s; *it && *it != c; it++);
    if ('\0' == *it) return NULL;
    return it;
}


void trim(char *str)
{
    char slen = strlen(str), i, j;
    for (i = 0; i < slen && str[i] != '\0'; i++) {
        if (str[i] == '\r' || str[i] == '\n' || str[i] == '\t' || str[i] == ' ' || str[i] == 127)
            for (j = i; j < slen && str[j] != '\0'; j++)
                str[j] = str[j + 1];
    }
}

void *_memcpy(void *dest, void *src, size_t cnt)
{
    char *it_src = src, *it_dst = dest;
    int i;
    for (i = 0; i < (cnt & 0xfffffffc); i+=4) {
        *(uint32_t*)it_dst = *(uint32_t*)it_src;
        it_src += 4;
        it_dst += 4;
    }
    for (; i < cnt; i++) {
        *it_dst = *it_src;
        it_dst++;
        it_src++;
    }
}

void *memcpy(void *dest, void *src, size_t cnt)
{
    return _memcpy(dest,src,cnt);
}

void *memset(void* dest, int val, size_t len)
{
    char *dst = dest;
    register uint32_t i, aux = val|(val << 8)|(val << 16)|(val << 24);
    for (i = 0; i < (len & 0xfffffffc); i += 4)
        *(uint32_t*)&dst[i] = aux;
    for (; i < len; i++)
        dst[i] = val;
    return dest;
}

char *strtok(char *str, const char delim)
{
    static char *it;
    char *prev;
    int i;

    if (str) {
        it = str;
    }
    if (it == NULL) return NULL;
    prev = it;
    for (i = 0; it[i] != '\0'; i++) {
        if (it[i] == delim) {
            it[i] = '\0';
            it = &it[i+1];
            return prev;
        }
    }
    it = NULL;
    return prev;
}
int memcmp(char *a, char *b, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        if (a[i] != b[i]) return 1;
    }
    return 0;
}

uint32_t swap32(uint32_t arg)
{
    return (((arg & 0xff) << 24)|((arg & 0xff00) << 8)|((arg & 0xff0000) >> 8)|((arg & 0xff000000) >> 24));
}

uint16_t swap16(uint16_t arg)
{
    return ((arg >> 8) | ((arg & 0xff) << 8));
}

#include <stdarg.h>

void strprintf(char *dest, const char *fmt, ...)
{
    char buf[11], space, pos; 
    int16_t dst_pos = 0;
    va_list args;

    va_start(args, fmt);
    for (; *fmt != '\0'; fmt++) {
        if (*fmt == '%') {
            uint16_t buf_len;
            char *aux;
            fmt++;
            if (*fmt == '\0') break;
            if (*fmt == '0') {
                space = 0; /* use '0' instead */
                fmt++;
            } else {
                space = 1;
            }
            if (*fmt >= '0' && *fmt <= '9') {
                pos = *fmt - '0';
                fmt++;
            } else {
                pos = 0;
            }
            switch (*fmt) {
                case 'd':
                    itoa(va_arg(args, int), buf, 10);
                    if (pos) {
                        int i;
                        for (i = 0; i < pos - strlen(buf); i++) {
                            if (space) {
                                dest[dst_pos++] = ' ';
                            } else {
                                dest[dst_pos++] = '0';
                            }
                        }
                    }
                    buf_len = strlen(buf);
                    _memcpy(&dest[dst_pos], buf, buf_len);
                    dst_pos += buf_len;
                break;
                case 'x':
                    itoa(va_arg(args, int), buf, 16);
                    if (pos) {
                        int i;
                        for (i = 0; i < pos - strlen(buf); i++) {
                            if (space) {
                                dest[dst_pos++] = ' ';
                            } else {
                                dest[dst_pos++] = '0';
                            }
                        }
                    }
                    buf_len = strlen(buf);
                    _memcpy(&dest[dst_pos], buf, buf_len);
                    dst_pos+=buf_len;
                break;
                case 's':
                    aux = va_arg(args, char*);
                    buf_len = strlen(aux);
                    _memcpy(&dest[dst_pos], aux, buf_len);
                    dst_pos+=buf_len;
                break;
                case 'c':
                    dest[dst_pos++] = va_arg(args, int);
                break;
                case '%':
                    dest[dst_pos++] = '%';
                break;
                default:
                    dest[dst_pos++] = *fmt;
            }
        } else {
            dest[dst_pos++] = *fmt;
        }
    }
    va_end(args);
    dest[dst_pos] = '\0';
}

