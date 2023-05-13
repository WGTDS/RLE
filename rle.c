/****************************************************************************
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <https://unlicense.org>
 ***************************************************************************/
/*---------------------------------------------------------------------------
    Run Length Encoding - FileIO Implementation

    Author  : White Guy That Don't Smile
    Date    : N/A
    License : UnLicense | Public Domain

    A free, generic RLE implementation written some years ago in C
    for, mostly, recreational use.

    This Model uses the Most Significant Bit from an RLE Code Byte,
    and the remaining 7 Bits to denote length.
    A leading bit of '1' denotes a proceeding string literal whereas
    a leading bit of '0' denotes duplication of the proceeding byte.
    Decompression is terminated when a NULL Code Byte is encountered,
    ideally, at the End Of File.
---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>



#define u16 unsigned short
#define r_int register int
#define DATA_INPUT 1
#define DATA_OUTPUT 0
static int buffer[128];
static struct {
    /* Output Sums */
    u16 oB, oK, oM, oG, oT, oP, oE, oZ, oY;
    /* Input Sums */
    u16 iB, iK, iM, iG, iT, iP, iE, iZ, iY;
} n;
#define frame (sizeof(n) / 4)
static u16 *n_ = (u16 *)&n,
           *_n = (u16 *)&n + frame;
static u16 **f_[2] = { &n_, &_n };



static void _updateSum(const int instance, r_int amount)
{
    register u16 *t = NULL, *p = NULL, *q = NULL;
/* establish target bisection '0' DATA_OUTPUT, '1' DATA_INPUT */
    t = **&f_[(instance == DATA_INPUT)];
    q = t + frame;
    p = t;
/* incrementally iterate through the specified sums section
   of the sums structure, and update subsections accordingly */
    while (amount--) {
        do {
            if (*t == 1000U) {
                *t++ %= 999U;
            }
            else {
                ++(*t);
                break;
            }
        } while (t < q);

        t = p;
    }

    return;
}



static void _appendSymbols(FILE *inFile,
                           const int amount,
                           r_int *index,
                           r_int *remaining)
{
    r_int c, i, j;

    for (i = 0, j = 0; i < amount; i++) {
        if (EOF == (c = fgetc(inFile))) {
            if ((i == 0) && (*remaining <= 0)) {
                *index = EOF;
            }

            goto ret;
        }
        else {
            buffer[(*index)++] = c;
            *index &= 0x7F;
            ++(*remaining);
            ++j;
        }
    }
ret:
    _updateSum(DATA_INPUT, j);
    return;
}



static void _cullSymbols(const int position, const int amount)
{
    r_int i;

    for (i = 0; i < amount; i++) {
/* iteratively assign a value distinct from any
   EOFs and characters within the buffer */
        buffer[(position + i) & 0x7F] = -32768;
    }

    return;
}



static void _decompressRLE(FILE *inFile, FILE *outFile)
{
    r_int symbol, length, i, j;
    int index = 0, remaining = 0;

    _appendSymbols(inFile, 128, &index, &remaining);

    for (i = 0; (EOF != index) && (0 != buffer[i]); i = (i + j) & 0x7F) {
        symbol = buffer[i] & 0x80;
        length = buffer[i++] & 0x7F;
        i &= 0x7F;
        j = 0;
        _updateSum(DATA_OUTPUT, length);

        do {
            fputc(buffer[((0 != symbol) ? ((i + j++) & 0x7F) : i)], outFile);
        } while (--length);

        j = ((0 != symbol) ? (j + 1) : 2);
        remaining -= j;
        _appendSymbols(inFile, j--, &index, &remaining);
    }

    return;
}



static void _compressRLE(FILE *inFile, FILE *outFile)
{
    r_int i, j, k;
    int index = 0, remaining = 0;
    int temp[128];

    _cullSymbols(0, 128);
    _appendSymbols(inFile, 127, &index, &remaining);

    for (i = 0; EOF != index; i = (i + j) & 0x7F) {
        for (j = 1;
             (buffer[i] == buffer[(i + j) & 0x7F]) && (j < 127);
             j++)
            ;

        if (j > 1) {
            temp[0] = j & 0x7F;
            temp[1] = buffer[i];
            j = 1;
        }
        else {
            for (j = 0;
                 (buffer[(i + j) & 0x7F] != buffer[(i + j + 1) & 0x7F]) && (j < 127);
                 j++) {
                temp[j + 1] = buffer[(i + j) & 0x7F];
            }

            temp[0] = (j & 0x7F) | 0x80;
        }

        for (k = 0; k <= j; k++) {
            fputc(temp[k], outFile);
        }

        _updateSum(DATA_OUTPUT, k);
        remaining -= (j = (temp[0] & 0x7F));
        _cullSymbols(i, j);
        _appendSymbols(inFile, j, &index, &remaining);
    }

    fputc(0, outFile);
    _updateSum(DATA_OUTPUT, 1);
    return;
}



int main(int argc, char *argv[])
{
    if (argc == 4) {
        FILE *inFile  = NULL,
             *outFile = NULL;
        char *s = NULL;

        if ((((s = argv[1], s[1] || NULL == strpbrk(s, "CDcd")) ||
              (s = argv[2], NULL == (inFile  = fopen(s, "rb")))) ||
              (s = argv[3], NULL == (outFile = fopen(s, "wb"))))) {
            if (NULL != outFile) {
                fclose(outFile);
            }
            if (NULL != inFile) {
                fclose(inFile);
            }

            printf("Argument Error:\n\"%s\"\n", s);
            return EXIT_FAILURE;
        }
        else {
            unsigned msec, sec;
            struct tm time;
            char format[12];
            void (*press)(FILE *, FILE *);
            char *msg[2] = { " bytes produced.", " bytes processed," };
        /* initialize I/O BKMGT... exponents with 0 */
            memset(&n, 0, sizeof(n));
            press = ('C' == toupper(*argv[1])) ? _compressRLE : _decompressRLE;
            press(inFile, outFile);
            msec = clock();
            sec = msec / CLOCKS_PER_SEC;
            memset(format, 0, 12);
            memset(&time, 0, sizeof(time));
            time.tm_yday = sec / 86400;
            time.tm_hour = sec / 3600;
            time.tm_min = (sec / 60) % 60;
            time.tm_sec = sec % 60;
            msec %= 1000;
            strftime(format, 12, "%Hh:%Mm:%Ss", &time);
            printf("\n");

            for (sec = 2; sec != 0; sec--) {
            /* make it easier to bisectionally dereference
               structural elements in the loop */
                u16 *z = **&f_[(sec / 2)] + frame, *a = z - frame, *Q;
            /* suppress leading zeros */
                for (Q = z; (*&Q[-1] == 0) && (Q > a); Q--);
            /* decrementally iterate through the input/output
               sums sections of the sums structure */
                while (z > a) {
                    if (&z[-1] < Q) {
                        printf("%.3u,", *&z[-1]);
                    }

                    --z;
                }

                printf("%s\n", msg[(sec / 2)]);
            }

            fflush(outFile);
            fclose(outFile);
            fclose(inFile);
            printf("Time Elapsed: %u day(s), %s;%.3ums\n",
                   time.tm_yday, format, msec);
            return EXIT_SUCCESS;
        }
    }
    else {
        printf("\nExpecting 4 arguments!\n\n"
               "rle <d/c> <infile> <outfile> [de]compress input to output\n");
        return EXIT_FAILURE;
    }
}
