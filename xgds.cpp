/*
 * xgds.cpp
 *
 *  Created on: Sep 13, 2017
 *      Author: xoiss
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>

uint16_t fread_u16(FILE *f) {
    uint16_t u16;
    if (fread(&u16, 2, 1, f) != 1) {
        fprintf(stderr, "end of file\n");
        exit(EXIT_SUCCESS);
    }
    return u16;
}

int main(void) {
    FILE *f = fopen("k1zhg454.gds", "rb");
    if (f == NULL) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        uint16_t u16 = fread_u16(f);
        if (u16 == 0) { // this is used only for multi-reel tapes
            fprintf(stderr, "padding between records\n");
            exit(EXIT_FAILURE); // continue;
        }

        u_int rec_len = ntohs(u16);
        if (rec_len & 0x1) {
            fprintf(stderr, "odd record length %u\n", (u_int)rec_len);
            exit(EXIT_FAILURE);
        }
        if (rec_len < 4) {
            fprintf(stderr, "insufficient record length %u\n", (u_int)rec_len);
            exit(EXIT_FAILURE);
        }

        u16 = fread_u16(f);
        u_int rec_type = (uint8_t)(u16 >> 0);
        u_int data_type = (uint8_t)(u16 >> 8);
        printf("%02X%02X %u\t:", rec_type, data_type, rec_len);

        rec_len -= 4;
        while (rec_len > 0) {
            u16 = fread_u16(f);
            u_int b0 = (uint8_t)(u16 >> 0);
            u_int b1 = (uint8_t)(u16 >> 8);
            printf("%02X-%02X ", b0, b1);
            rec_len -= 2;
        }
        printf("$\n");
    }

    fclose(f);
    return EXIT_SUCCESS;
}

/*

Records are always an even number of bytes long. If a character string is an
odd number of bytes long it is padded with a null character.

Minimal set of record types:
0, HEADER, 0x0002
1, BGNLIB, 0x0102
2, LIBNAME, 0x0206
3, UNITS, 0x0305
4, ENDLIB, 0x0400
5, BGNSTR, 0x0502
6, STRNAME, 0x0606 -- parse data
7, ENDSTR, 0x0700
8, BOUNDARY, 0x0800
10, SREF, 0x0A00
13, LAYER, 0x0D02 -- parse data
14, DATATYPE, 0x0E02
16, XY, 0x1003 -- parse data
17, ENDEL, 0x1100
18, SNAME, 0x1206 -- parse data

Minimal set of data types:
0x00, void
0x02, int16_t
0x03, int32_t
0x06, string

All odd length strings must be padded with a null character (the number zero)
and the byte count for the record containing the ASCII string must include this
null character. Stream read-in programs must look for the null character and
decrease the length of the string by one if the null character is present.

Minimal set of language structures:
<stream format> ::= HEADER BGNLIB LIBNAME UNITS {<structure>}* ENDLIB
<structure> ::= BGNSTR STRNAME {<element>}* ENDSTR
<element> ::= {<boundary> | <SREF>} ENDEL
<boundary> ::= BOUNDARY LAYER DATATYPE XY
<SREF> ::= SREF SNAME XY

*/
