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



#define DATA_INPUT 1
#define DATA_OUTPUT 0
static int buffer[128];
static struct {
	/* Input Sums */
	unsigned short iB, iK, iM, iG, iT, iP, iE, iY, iZ;
	/* Output Sums */
	unsigned short oB, oK, oM, oG, oT, oP, oE, oY, oZ;
} n;
#define frame (sizeof(n) / 4)



static void _updateSum(const int instance)
{
	register unsigned short *t = NULL, *q = NULL;
/* establish target bisection */
	if (instance == DATA_INPUT) {
		t = (unsigned short *)&n;
	}
	else {
		t = (unsigned short *)&n + frame;
	}

	q = t + frame;
/* incrementally iterate through the specified
   sums section of the sums structure,
   and update subsections accordingly */
	while (t < q) {
		if (*t == 1000U) {
			*t++ %= 999U;
		}
		else {
			(*t)++;
			break;
		}
	}

	return;
}



static void _appendSymbols(FILE *inFile,
                           int amount,
						   register int *index,
						   register int *remaining)
{
	register int c, i;

	for (i = 0; i < amount; i++) {
		if (EOF == (c = fgetc(inFile))) {
			if ((i == 0) && (*remaining <= 0)) {
				*index = EOF;
			}

			return;
		}
		else {
			buffer[(*index)++] = c;
			*index &= 0x7F;
			(*remaining)++;
			_updateSum(DATA_INPUT);
		}
	}

	return;
}



static void _cullSymbols(int position, int amount)
{
	register int i;

	for (i = 0; i < amount; i++) {
/* iteratively assign a value distinct from any
   EOFs and characters within the buffer */
		buffer[(position + i) & 0x7F] = -32768;
	}

	return;
}



static void _decompressRLE(FILE *inFile, FILE *outFile)
{
	register int symbol, length, i, j;
	int index = 0, remaining = 0;

	_appendSymbols(inFile, 128, &index, &remaining);

	for (i = 0; (EOF != index) && (0 != buffer[i]); i = (i + j) & 0x7F) {
		symbol = buffer[i] & 0x80;
		length = buffer[i++] & 0x7F;
		i &= 0x7F;
		j = 0;

		do {
			fputc(buffer[((0 != symbol) ? ((i + j++) & 0x7F) : i)], outFile);
			_updateSum(DATA_OUTPUT);
		} while (--length);

		j = ((0 != symbol) ? (j + 1) : 2);
		remaining -= j;
		_appendSymbols(inFile, j--, &index, &remaining);
	}

	return;
}



static void _compressRLE(FILE *inFile, FILE *outFile)
{
	register int i, j, k;
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
			_updateSum(DATA_OUTPUT);
		}

		remaining -= (j = (temp[0] & 0x7F));
		_cullSymbols(i, j);
		_appendSymbols(inFile, j, &index, &remaining);
	}

	fputc(0, outFile);
	_updateSum(DATA_OUTPUT);
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
			s = NULL;
			return EXIT_FAILURE;
		}
		else {
			void (*press)(FILE *, FILE *);
		/* make it easier to bisectionally dereference
		   structural elements in a loop */
			unsigned short *z = (unsigned short *)&n,
			               *i = z + frame,
						   *o = i + frame;
		/* for trimming the fat made by printf() below */
			int prevexp = 0, check = 0;
		/* initialize I/O BKMGT... exponents with 0 */
			memset(&n, 0, sizeof(n));
			press = ('C' == toupper(*argv[1])) ? _compressRLE : _decompressRLE;
			press(inFile, outFile);
			printf("\n");
		/* decrementally iterate through the input sums
		   section of the sums structure */
			while (i > z) {
				prevexp = (*&i[-1] != 0);
				check = ((prevexp == 0) && (i != z + 1));
				printf((check == 1) ? "" : "%.3d,", *&i[-1]);
				--i;
			}

			i = z + frame;
			prevexp ^= prevexp;
			printf(" bytes processed,\n");
		/* decrementally iterate through the output sums
		   section of the sums structure */
			while (o > i) {
				prevexp = (*&o[-1] != 0);
				check = ((prevexp == 0) && (o != i + 1));
				printf((check == 1) ? "" : "%.3d,", *&o[-1]);
				--o;
			}

			printf(" bytes produced.\n");
			fflush(outFile);
			fclose(outFile);
			fclose(inFile);
			s = NULL;
			return EXIT_SUCCESS;
		}
	}
	else {
		printf("\nExpecting 4 arguments!\n\n"
               "rle <d/c> <infile> <outfile> [de]compress input to output\n");
		return EXIT_FAILURE;
	}
}
