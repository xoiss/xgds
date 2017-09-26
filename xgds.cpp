/*
 * xgds.cpp
 *
 *  Created on: Sep 13, 2017
 *      Author: xoiss
 */

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int parse_gdsii(FILE *f);
static int fread_u16(FILE *f, uint16_t *u16);

int main(void) {
    FILE *f = fopen("k1zhg454.gds", "rb");
    if (f == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }
    int rc = parse_gdsii(f);
    fclose(f);
    return rc;
}

/*

If a character string is an odd number of bytes long it is padded with a null
character.

All odd length strings must be padded with a null character (the number zero)
and the byte count for the record containing the ASCII string must include this
null character. Stream read-in programs must look for the null character and
decrease the length of the string by one if the null character is present.

*/

static int parse_gdsii(FILE *f) {
    unsigned eof = 0;
    do {
        uint16_t u16;
        if (fread_u16(f, &u16) != EXIT_SUCCESS) {
            return EXIT_FAILURE;
        }
        if (u16 == 0) { // this is used only for multi-reel tapes
            fprintf(stderr, "padding between records\n");
            return EXIT_FAILURE;
        }
        u_int rec_len = ntohs(u16);
        if (rec_len & 0x1) {
            fprintf(stderr, "odd record length %u\n", rec_len);
            return EXIT_FAILURE;
        }
        if (rec_len < 4) {
            fprintf(stderr, "insufficient record length %u\n", rec_len);
            return EXIT_FAILURE;
        }
        if (fread_u16(f, &u16) != EXIT_SUCCESS) {
            return EXIT_FAILURE;
        }
        u_int rec_tag = ntohs(u16);
        static enum {
            HEADER = 0,
            BGNLIB,
            LIBNAME,
            UNITS,
            ENDLIB_OR_STRUCTURE,
            STRNAME,
            ENDSTR_OR_ELEMENT,
            LAYER,
            DATATYPE,
            XY_IN_BOUNDARY,
            SNAME,
            XY_IN_SREF,
            ENDEL,
        } state = HEADER;
        switch (state) {
        case HEADER:
            if (rec_tag == 0x0002) {
                printf("HEADER\n");
                state = BGNLIB;
            }
            break;
        case BGNLIB:
            if (rec_tag == 0x0102) {
                printf("BGNLIB\n");
                state = LIBNAME;
            }
            break;
        case LIBNAME:
            if (rec_tag == 0x0206) {
                printf("LIBNAME\n");
                state = UNITS;
            }
            break;
        case UNITS:
            if (rec_tag == 0x0305) {
                printf("UNITS\n");
                state = ENDLIB_OR_STRUCTURE;
            }
            break;
        case ENDLIB_OR_STRUCTURE:
            if (rec_tag == 0x0400) {            // ENDLIB
                printf("ENDLIB\n");
                eof = 1;
            } else if (rec_tag == 0x0502) {     // BGNSTR
                printf("BGNSTR\n");
                state = STRNAME;
            }
            break;
        case STRNAME:
            if (rec_tag == 0x0606) {
                printf("STRNAME\n");
                state = ENDSTR_OR_ELEMENT;
            }
            break;
        case ENDSTR_OR_ELEMENT:
            if (rec_tag == 0x0700) {            // ENDSTR
                printf("ENDSTR\n");
                state = ENDLIB_OR_STRUCTURE;
            } else if (rec_tag == 0x0800) {     // BOUNDARY
                printf("BOUNDARY\n");
                state = LAYER;
            } else if (rec_tag == 0x0A00) {     // SREF
                printf("SREF\n");
                state = SNAME;
            }
            break;
        case LAYER:
            if (rec_tag == 0x0D02) {
                printf("LAYER\n");
                state = DATATYPE;
            }
            break;
        case DATATYPE:
            if (rec_tag == 0x0E02) {
                printf("DATATYPE\n");
                state = XY_IN_BOUNDARY;
            }
            break;
        case XY_IN_BOUNDARY:
            if (rec_tag == 0x1003) {
                printf("XY\n");
                state = ENDEL;
            }
            break;
        case SNAME:
            if (rec_tag == 0x1206) {
                printf("SNAME\n");
                state = XY_IN_SREF;
            }
            break;
        case XY_IN_SREF:
            if (rec_tag == 0x1003) {
                printf("XY\n");
                state = ENDEL;
            }
            break;
        case ENDEL:
            if (rec_tag == 0x1100) {
                printf("ENDEL\n");
                state = ENDSTR_OR_ELEMENT;
            }
            break;
        default:
            fprintf(stderr, "unknown record tag %04X\n", rec_tag);
            return EXIT_FAILURE;
        }
        rec_len -= 4;
        while (rec_len > 0) {
            if (fread_u16(f, &u16) != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
            rec_len -= 2;
        }
    } while (!eof);
    return EXIT_SUCCESS;
}

static int fread_u16(FILE *f, uint16_t *u16) {
    if (fread(u16, 2, 1, f) != 1) {
        if (feof(f)) {
            fprintf(stderr, "unexpected end of file\n");
        } else {
            perror("fread");
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
