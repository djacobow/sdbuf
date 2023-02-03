#include <stdio.h>
#include <ctype.h>
#include "debug.h"

static uint32_t errors = 0;

uint32_t getErrors() {
    return errors;
}

int8_t show_err(int8_t rv, const char *msg) {
    if (rv) {
        errors += 1;
        printf("err: %s, rv: %d\n",msg,rv);
    }
    return rv;
}

char printOrDot(char *b) {
    return isprint(*b) ? *b : '.';
}

void dumpBuffer(void *b, size_t l) {
    printf("\ndumpBuffer\n");
    for (size_t i=0;i<l;i+= 16) {
        printf("%04x : %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x   %c%c%c%c %c%c%c%c %c%c%c%c %c%c%c%c\n",
            (uint32_t)i,
            *((uint8_t *)b + i + 0),
            *((uint8_t *)b + i + 1),
            *((uint8_t *)b + i + 2),
            *((uint8_t *)b + i + 3),
            *((uint8_t *)b + i + 4),
            *((uint8_t *)b + i + 5),
            *((uint8_t *)b + i + 6),
            *((uint8_t *)b + i + 7),
            *((uint8_t *)b + i + 8),
            *((uint8_t *)b + i + 9),
            *((uint8_t *)b + i + 10),
            *((uint8_t *)b + i + 11),
            *((uint8_t *)b + i + 12),
            *((uint8_t *)b + i + 13),
            *((uint8_t *)b + i + 14),
            *((uint8_t *)b + i + 15),
            printOrDot((char *)b + i + 0),
            printOrDot((char *)b + i + 1),
            printOrDot((char *)b + i + 2),
            printOrDot((char *)b + i + 3),
            printOrDot((char *)b + i + 4),
            printOrDot((char *)b + i + 5),
            printOrDot((char *)b + i + 6),
            printOrDot((char *)b + i + 7),
            printOrDot((char *)b + i + 8),
            printOrDot((char *)b + i + 9),
            printOrDot((char *)b + i + 10),
            printOrDot((char *)b + i + 11),
            printOrDot((char *)b + i + 12),
            printOrDot((char *)b + i + 13),
            printOrDot((char *)b + i + 14),
            printOrDot((char *)b + i + 15)
        );
    } 
}

#include <execinfo.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

static void full_write(int fd, const char *buf, size_t len)
{
        while (len > 0) {
                ssize_t ret = write(fd, buf, len);

                if ((ret == -1) && (errno != EINTR))
                        break;

                buf += (size_t) ret;
                len -= (size_t) ret;
        }
}

void print_backtrace(void)
{
        static const char start[] = "BACKTRACE ------------\n";
        static const char end[] = "----------------------\n";

        void *bt[1024];
        int bt_size;
        char **bt_syms;
        int i;

        bt_size = backtrace(bt, 1024);
        bt_syms = backtrace_symbols(bt, bt_size);
        full_write(STDERR_FILENO, start, strlen(start));
        for (i = 1; i < bt_size; i++) {
                size_t len = strlen(bt_syms[i]);
                full_write(STDERR_FILENO, bt_syms[i], len);
                full_write(STDERR_FILENO, "\n", 1);
        }
        full_write(STDERR_FILENO, end, strlen(end));
    free(bt_syms);
}
