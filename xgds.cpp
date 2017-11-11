/*
 * xgds.cpp
 *
 *  Created on: Sep 13, 2017
 *      Author: xoiss
 */

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "classes.h"

static int parse_gdsii(FILE *f);
static int parse_struct_name(FILE *f, u_int *data_len, xgds::Name **name);
static int parse_layer_id(FILE *f, u_int *data_len, xgds::Layer **layer);
static int parse_single_point(FILE *f, u_int *data_len, xgds::Point **point);
static int parse_points_chain(FILE *f, u_int *data_len, xgds::Chain **chain);
static int check_rec_tag(u_int rec_tag, u_int must_be);
static int check_data_len(u_int data_len, u_int must_be);
static int fread_u16(FILE *f, uint16_t *u16, u_int data_len);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "missed input gds file\n");
        return EXIT_FAILURE;
    }
    FILE *f = fopen(argv[1], "rb");
    if (f == NULL) {
        perror("fopen");
        return EXIT_FAILURE;
    }
    int rc = parse_gdsii(f);
    fclose(f);
    return rc;
}

static int parse_gdsii(FILE *f) {
    unsigned eof = 0;
    do {
        uint16_t u16;
        if (fread_u16(f, &u16, 1) != EXIT_SUCCESS) {
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
        u_int data_len = rec_len / 2 - 2;
        if (fread_u16(f, &u16, 1) != EXIT_SUCCESS) {
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
            case HEADER: {
                if (check_rec_tag(rec_tag, 0x0002) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("HEADER\n");
                if (check_data_len(data_len, 1) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                state = BGNLIB;
                break;
            }
            case BGNLIB: {
                if (check_rec_tag(rec_tag, 0x0102) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("BGNLIB\n");
                if (check_data_len(data_len, 6 * 2) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                state = LIBNAME;
                break;
            }
            case LIBNAME: {
                if (check_rec_tag(rec_tag, 0x0206) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("LIBNAME\n");
                state = UNITS;
                break;
            }
            case UNITS: {
                if (check_rec_tag(rec_tag, 0x0305) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("UNITS\n");
                if (check_data_len(data_len, (8 / 2) * 2) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                state = ENDLIB_OR_STRUCTURE;
                break;
            }
            case ENDLIB_OR_STRUCTURE: {
                if (rec_tag == 0x0400) {            // ENDLIB
                    printf("ENDLIB\n");
                    if (check_data_len(data_len, 0) != EXIT_SUCCESS) {
                        return EXIT_FAILURE;
                    }
                    eof = 1;
                    int fd = fileno(f);
                    if (fd == -1) {
                        perror("fileno");
                        return EXIT_FAILURE;
                    }
                    struct stat sb;
                    if (fstat(fd, &sb) == -1) {
                        perror("fstat");
                        return EXIT_FAILURE;
                    }
                    long pos = ftell(f);
                    if (pos < sb.st_size) {
                        fprintf(stderr, "extra bytes after ENDLIB, "
                                "bytes read %li, file size %li\n",
                                pos, sb.st_size);
                        return EXIT_FAILURE;
                    }
                } else if (rec_tag == 0x0502) {     // BGNSTR
                    printf("BGNSTR\n");
                    if (check_data_len(data_len, 6 * 2) != EXIT_SUCCESS) {
                        return EXIT_FAILURE;
                    }
                    state = STRNAME;
                } else {
                    fprintf(stderr, "invalid record tag 0x%04X, must be "
                            "either of 0x0400, 0x0502\n", rec_tag);
                    return EXIT_FAILURE;
                }
                break;
            }
            case STRNAME: {
                if (check_rec_tag(rec_tag, 0x0606) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("STRNAME:");
                xgds::Name *name;
                if (parse_struct_name(f, &data_len, &name) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("\n");
                state = ENDSTR_OR_ELEMENT;
                break;
            }
            case ENDSTR_OR_ELEMENT: {
                if (rec_tag == 0x0700) {            // ENDSTR
                    printf("ENDSTR\n");
                    if (check_data_len(data_len, 0) != EXIT_SUCCESS) {
                        return EXIT_FAILURE;
                    }
                    state = ENDLIB_OR_STRUCTURE;
                } else if (rec_tag == 0x0800) {     // BOUNDARY
                    printf("BOUNDARY\n");
                    if (check_data_len(data_len, 0) != EXIT_SUCCESS) {
                        return EXIT_FAILURE;
                    }
                    state = LAYER;
                } else if (rec_tag == 0x0A00) {     // SREF
                    printf("SREF\n");
                    if (check_data_len(data_len, 0) != EXIT_SUCCESS) {
                        return EXIT_FAILURE;
                    }
                    state = SNAME;
                } else {
                    fprintf(stderr, "invalid record tag 0x%04X, must be "
                            "either of 0x0700, 0x0800, 0x0A00\n", rec_tag);
                    return EXIT_FAILURE;
                }
                break;
            }
            case LAYER: {
                if (check_rec_tag(rec_tag, 0x0D02) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("LAYER:");
                xgds::Layer *layer;
                if (parse_layer_id(f, &data_len, &layer) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("\n");
                state = DATATYPE;
                break;
            }
            case DATATYPE: {
                if (check_rec_tag(rec_tag, 0x0E02) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("DATATYPE\n");
                if (check_data_len(data_len, 1) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                state = XY_IN_BOUNDARY;
                break;
            }
            case XY_IN_BOUNDARY: {
                if (check_rec_tag(rec_tag, 0x1003) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("XY:");
                xgds::Chain *chain;
                if (parse_points_chain(f, &data_len, &chain) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("\n");
                u_int chain_len = chain->len;
                if (chain_len < 3) {
                    fprintf(stderr, "invalid points chain length %u, "
                            "must be at least 3\n", chain_len);
                    return EXIT_FAILURE;
                }
                xgds::Point &first_point = (*chain)[0];
                xgds::Point &last_point = (*chain)[chain_len - 1];
                if (first_point != last_point) {
                    fprintf(stderr, "points chain is not closed: "
                            "first point (%i,%i), last point (%i,%i)\n",
                            first_point.x, first_point.y,
                            last_point.x, last_point.y);
                    return EXIT_FAILURE;
                }
                state = ENDEL;
                break;
            }
            case SNAME: {
                if (check_rec_tag(rec_tag, 0x1206) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("SNAME:");
                xgds::Name *name;
                if (parse_struct_name(f, &data_len, &name) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("\n");
                state = XY_IN_SREF;
                break;
            }
            case XY_IN_SREF: {
                if (check_rec_tag(rec_tag, 0x1003) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("XY:");
                xgds::Point *point;
                if (parse_single_point(f, &data_len, &point) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("\n");
                state = ENDEL;
                break;
            }
            case ENDEL: {
                if (check_rec_tag(rec_tag, 0x1100) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                printf("ENDEL\n");
                if (check_data_len(data_len, 0) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
                state = ENDSTR_OR_ELEMENT;
                break;
            }
            default: {
                fprintf(stderr, "invalid parser state %i\n", state);
                return EXIT_FAILURE;
            }
        }
        while (data_len-- > 0) {
            if (fread_u16(f, &u16, 1) != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
        }
    } while (!eof);
    return EXIT_SUCCESS;
}

static int parse_struct_name(FILE *f, u_int *data_len, xgds::Name **name) {
    u_int chars_num = *data_len * 2;
    if (chars_num == 0) {
        fprintf(stderr, "empty structure name\n");
        return EXIT_FAILURE;
    }
    if (chars_num > 32) {
        fprintf(stderr, "structure name is too long, length %u\n", chars_num);
        return EXIT_FAILURE;
    }
    char struct_name[33];
    memset(struct_name, '\0', sizeof(struct_name));
    if (fread_u16(f, (uint16_t*)struct_name, *data_len) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (struct_name[chars_num - 1] == '\0') {
        chars_num--;
    }
    for (int i = 0; i < chars_num; ++i) {
        char c = struct_name[i];
        if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z') &&
                (c < '0' || c > '9') && c != '_' && c != '?' && c != '$') {
            fprintf(stderr, "invalid character '%c' (0x%02hhX)\n",
                    isprint(c) ? c : '?', c);
            return EXIT_FAILURE;
        }
    }
    *name = new xgds::Name(chars_num, struct_name);
    printf(" [%u] '%s'", chars_num, struct_name);
    *data_len = 0;
    return EXIT_SUCCESS;
}

static int parse_layer_id(FILE *f, u_int *data_len, xgds::Layer **layer) {
    if (check_data_len(*data_len, 1) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    uint16_t u16;
    if (fread_u16(f, &u16, 1) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    int layer_id = (int16_t)ntohs(u16);
    if (layer_id < 0 || layer_id > 63) {
        fprintf(stderr, "invalid layer %i\n", layer_id);
        return EXIT_FAILURE;
    }
    *layer = new xgds::Layer(layer_id);
    printf(" %i", layer_id);
    *data_len = 0;
    return EXIT_SUCCESS;
}

static int parse_single_point(FILE *f, u_int *data_len, xgds::Point **point) {
    if (check_data_len(*data_len, 4) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    uint32_t u32[2];
    if (fread_u16(f, (uint16_t*)u32, 4) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    int x, y;
    x = (int32_t)ntohl(u32[0]);
    y = (int32_t)ntohl(u32[1]);
    *point = new xgds::Point(x, y);
    printf(" (%i,%i)", x, y);
    *data_len = 0;
    return EXIT_SUCCESS;
}

static int parse_points_chain(FILE *f, u_int *data_len, xgds::Chain **chain) {
    if (*data_len % 4 != 0) {
        fprintf(stderr, "data length %u is not a multiple of point size\n",
                *data_len);
        return EXIT_FAILURE;
    }
    u_int points_num = *data_len / (2 * 2);
    if (points_num == 0) {
        fprintf(stderr, "empty points array\n");
        return EXIT_FAILURE;
    }
    printf(" [%u]", points_num);
    *chain = new xgds::Chain(points_num);
    while (points_num-- > 0) {
        uint32_t u32[2];
        if (fread_u16(f, (uint16_t*)u32, 4) != EXIT_SUCCESS) {
            return EXIT_FAILURE;
        }
        int x, y;
        x = (int32_t)ntohl(u32[0]);
        y = (int32_t)ntohl(u32[1]);
        (*chain)->load(x, y);
        printf(" (%i,%i)", x, y);
    }
    *data_len = 0;
    return EXIT_SUCCESS;
}

static int check_rec_tag(u_int rec_tag, u_int must_be) {
    if (rec_tag != must_be) {
        fprintf(stderr, "invalid record tag 0x%04X, must be 0x%04X\n",
                rec_tag, must_be);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int check_data_len(u_int data_len, u_int must_be) {
    if (data_len != must_be) {
        fprintf(stderr, "invalid data length %u, must be %u\n",
                data_len, must_be);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int fread_u16(FILE *f, uint16_t *u16, u_int data_len) {
    if (fread(u16, 2, data_len, f) != data_len) {
        if (feof(f)) {
            fprintf(stderr, "unexpected end of file\n");
        } else {
            perror("fread");
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
