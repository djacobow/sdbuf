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


