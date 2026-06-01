#include "common.h"

/* 0x108e70 — Bit vector AND.
 *
 * Computes the bitwise AND of two equal-size bit vectors,
 * writing the result to result_out if non-null.
 * Returns 1 if any word in the result is non-zero, 0 otherwise.
 */
char bit_vector_and(short bit_vector_size, int v0, int v1, int result_out)
{
	unsigned int *v0_words;
	unsigned int *v1_words;
	unsigned int *out_words;
	unsigned int word;
	int num_words;
	int i;
	char any_nonzero;

	if (v0 == 0 || v1 == 0) {
		display_assert("v0 && v1",
			"c:\\halo\\SOURCE\\math\\integer_math.c", 0x15c, 1);
		system_exit(-1);
	}

	any_nonzero = 0;
	v0_words = (unsigned int *)v0;
	v1_words = (unsigned int *)v1;
	out_words = (unsigned int *)result_out;
	num_words = (bit_vector_size + 31) >> 5;

	for (i = num_words - 1; i >= 0; i--) {
		word = v0_words[i] & v1_words[i];
		if (result_out != 0) {
			out_words[i] = word;
		}
		if (word != 0) {
			any_nonzero = 1;
		}
	}

	return any_nonzero;
}

/* 0x108f00 — Bit vector OR.
 *
 * Computes the bitwise OR of two equal-size bit vectors,
 * storing the result in result_out.
 */
void bit_vector_or(short bit_vector_size, int v0, int v1, int result_out)
{
	unsigned int *v0_words;
	unsigned int *v1_words;
	unsigned int *out_words;
	short num_words;
	short i;

	if (v0 == 0 || v1 == 0) {
		display_assert("v0 && v1",
			"c:\\halo\\SOURCE\\math\\integer_math.c", 0x171, 1);
		system_exit(-1);
	}

	if (result_out == 0) {
		display_assert("result",
			"c:\\halo\\SOURCE\\math\\integer_math.c", 0x172, 1);
		system_exit(-1);
	}

	v0_words = (unsigned int *)v0;
	v1_words = (unsigned int *)v1;
	out_words = (unsigned int *)result_out;
	num_words = (short)((bit_vector_size + 31) >> 5);

	for (i = num_words - 1; i >= 0; i--) {
		out_words[i] = v0_words[i] | v1_words[i];
	}
}

/* 0x108fa0 — Bit vector NOT/complement.
 *
 * Stores the bitwise complement (NOT) of vector into result_out.
 */
void FUN_00108fa0(short bit_vector_size, int vector, int result_out)
{
	unsigned int *vec_words;
	unsigned int *out_words;
	int num_words;
	int i;

	if (vector == 0 || result_out == 0) {
		display_assert("vector && result",
			"c:\\halo\\SOURCE\\math\\integer_math.c", 0x183, 1);
		system_exit(-1);
	}

	vec_words = (unsigned int *)vector;
	out_words = (unsigned int *)result_out;
	num_words = (bit_vector_size + 31) >> 5;

	for (i = num_words - 1; i >= 0; i--) {
		out_words[i] = ~vec_words[i];
	}
}
