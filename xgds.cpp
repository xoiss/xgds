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
        if (rec_len == 0) {
            printf("!zero!\n");
        }
        printf("%02X%02X %u\n", rec_type, data_type, rec_len);

        if (fseek(f, rec_len - 4, SEEK_CUR) != 0) {
            perror("fseek failed");
            exit(EXIT_FAILURE);
        }
    }

    fclose(f);
    return EXIT_SUCCESS;
}

/*

Records are always an even number of bytes long. If a character string is an
odd number of bytes long it is padded with a null character.

0, HEADER, 0x0002
1, BGNLIB, 0x0102
2, LIBNAME, 0x0206
3, UNITS, 0x0305
4, ENDLIB, 0x0400
5, BGNSTR, 0x0502
6, STRNAME, 0x0606
7, ENDSTR, 0x0700
8, BOUNDARY, 0x0800
9, PATH, 0x0900
10, SREF, 0x0A00
11, AREF, 0x0B00
12, TEXT, 0x0C00
13, LAYER, 0x0D02
14, DATATYPE, 0x0E02
15, WIDTH, 0x0F03
16, XY, 0x1003
17, ENDEL, 0x1100
18, SNAME, 0x1206
19, COLROW, 0x1302
20, TEXTNODE, 0x1400
21, NODE, 0x1500
22, TEXTTYPE, 0x1602
23, PRESENTATION, 0x1701
24, SPACING, 0x18
25, STRING, 0x1906
26, STRANS, 0x1A01
27, MAG, 0x1B05
28, ANGLE, 0x1C05
29, UINTEGER, 0x1D
30, USTRING, 0x1E
31, REFLIBS, 0x1F06
32, FONTS, 0x2006
33, PATHTYPE, 0x2102
34, GENERATIONS, 0x2202
35, ATTRTABLE, 0x2306
36, STYPTABLE, 0x2406
37, STRTYPE, 0x2502
38, ELFLAGS, 0x2601
39, ELKEY, 0x2703
40, LINKTYPE, 0x28
41, LINKKEYS, 0x29
42, NODETYPE, 0x2A02
43, PROPATTR, 0x2B02
44, PROPVALUE, 0x2C06
45, BOX, 0x2D00
46, BOXTYPE, 0x2E02
47, PLEX, 0x2F03
48, BGNEXTN, 0x3003
49, ENDEXTN, 0x3103
50, TAPENUM, 0x3202
51, TAPECODE, 0x3302
52, STRCLASS, 0x3401
53, RESERVED, 0x3503
54, FORMAT, 0x3602
55, MASK, 0x3706
56, ENDMASKS, 0x3800
57, LIBDIRSIZE, 0x3902
58, SRFNAME, 0x3A06
59, LIBSECUR, 0x3B02

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
