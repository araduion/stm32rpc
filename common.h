/*
Owner = Antonescu Radu-Ion
Year = 2016

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

#define NULL 0

typedef enum { false = 0, true = 1 } bool;
//typedef unsigned char uint8_t;
//typedef unsigned int uint32_t;
typedef unsigned long int size_t;

char *strcpy (char *dest, const char *src);
char *strchr(char *str, int ch);
int strcmp(const char *str1, const char *str2);
int strncmp(const char *str1, const char *str2, size_t len);
int strlen(const char *str);
void itoa(int i, char *buf, int base);
char *_strchr(const char *s, int c);
int pow_int(char exp, int base);
int pow_int(char exp, int base);
int atoi16(const char *str);
void trim(char *str);
void *_memcpy(void *dest, void *src, size_t cnt);
void *memset(void* dest, int val, size_t len);
int memcmp(char *a, char *b, int size);
uint32_t swap32(uint32_t arg);
uint16_t swap16(uint16_t arg);

#ifndef htons
#define htons(a) swap16(a)
#define ntohs(a) swap16(a)
#endif

#ifndef htonl
#define htonl(a) swap32(a)
#define ntohl(a) swap32(a)
#endif

#endif


