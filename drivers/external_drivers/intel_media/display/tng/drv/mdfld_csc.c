/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Jim Liu <jim.liu@intel.com>
 */

#include <drm/drmP.h>
#include "psb_drv.h"
#include "psb_intel_reg.h"
#include <linux/math64.h>

#define MULTIPLIER_CHROM 10000
#define MULTIPLIER_MATRIX 10000
#define MULTIPLIER_MATRIX1 100000
#define MULTIPLIER_MATRIX2 100
#define csc_sign(x) ((x < 0) ? false : true)

/**
 *  csc_matrix_mult_func
 *
 *
 */
static int64_t csc_matrix_mult_func(int64_t * M1, int64_t * M2)
{
	int64_t result = 0;
	int i = 0;

	for (i = 0; i < 3; i++)
		result += M1[i] * M2[i];

	return result;
}

/**
 *  csc_matrix_mult
 *
 *  3x3 matrix multiply 3x3 matrix
 *
 */
static void csc_matrix_mult(int64_t * M1, int64_t * M2, int64_t * M3)
{
	int64_t temp1[3], temp2[3];
	int i = 0;

	for (i = 0; i < 3; i++) {
		temp1[i] = M1[i];
		temp2[i] = M2[i * 3];
	}

	M3[0] = csc_matrix_mult_func(temp1, temp2);

	for (i = 0; i < 3; i++) {
		temp1[i] = M1[i];
		temp2[i] = M2[(i * 3) + 1];
	}

	M3[1] = csc_matrix_mult_func(temp1, temp2);

	for (i = 0; i < 3; i++) {
		temp1[i] = M1[i];
		temp2[i] = M2[(i * 3) + 2];
	}

	M3[2] = csc_matrix_mult_func(temp1, temp2);

	for (i = 0; i < 3; i++) {
		temp1[i] = M1[i + 3];
		temp2[i] = M2[i * 3];
	}

	M3[3] = csc_matrix_mult_func(temp1, temp2);

	for (i = 0; i < 3; i++) {
		temp1[i] = M1[i + 3];
		temp2[i] = M2[(i * 3) + 1];
	}

	M3[4] = csc_matrix_mult_func(temp1, temp2);

	for (i = 0; i < 3; i++) {
		temp1[i] = M1[i + 3];
		temp2[i] = M2[(i * 3) + 2];
	}

	M3[5] = csc_matrix_mult_func(temp1, temp2);

	for (i = 0; i < 3; i++) {
		temp1[i] = M1[i + 6];
		temp2[i] = M2[i * 3];
	}

	M3[6] = csc_matrix_mult_func(temp1, temp2);

	for (i = 0; i < 3; i++) {
		temp1[i] = M1[i + 6];
		temp2[i] = M2[(i * 3) + 1];
	}

	M3[7] = csc_matrix_mult_func(temp1, temp2);

	for (i = 0; i < 3; i++) {
		temp1[i] = M1[i + 6];
		temp2[i] = M2[(i * 3) + 2];
	}

	M3[8] = csc_matrix_mult_func(temp1, temp2);
}

/**
 *  csc_div64
 *
 *  division with 64bit dividend and 64bit divisor.
 *
 */
static int64_t csc_div64(int64_t divident, int64_t divisor)
{
	bool sign = !(csc_sign(divident) ^ csc_sign(divisor));
	uint64_t temp_N = (uint64_t) - 1;
	uint64_t temp_divid = 0, temp_divis = 0, temp_quot = 0;

	if (divident < 0) {
		temp_divid = temp_N - ((uint64_t) divident) + 1;
	} else {
		temp_divid = (uint64_t) divident;
	}

	if (divisor < 0) {
		temp_divis = temp_N - ((uint64_t) divisor) + 1;
	} else {
		temp_divis = (uint64_t) divisor;
	}

	temp_quot = div64_u64(temp_divid, temp_divis);

	if (!sign)
		temp_quot = temp_N - ((uint64_t) temp_quot) + 1;

	return (int64_t) temp_quot;
}

/**
 *  csc_det_2x2_matrix
 *
 *  get the determinant of 2x2 matrix
 *
 *  note: the 2x2 matrix will be represented in a vector with 4 elements.
 *  M00 = V0, M01 = V1, M10 = V2, M11 = V3.
 *  det M = V0 * V3 - V1 * V2
 */
int64_t csc_det_2x2_matric(int64_t * M2)
{
	return ((M2[0] * M2[3]) - (M2[1] * M2[2]));
}

/**
 *  csc_det_3x3_matrix
 *
 *  get the determinant of 3x3 matrix
 *
 *  note: the 3x3 matrix will be represented in a vector with 9 elements.
 *  M00 = V0, M01 = V1, M02 = V2, M10 = V3, M11 = V4, M12 = V5, M20 = V6, M21
 *  = V7, M22 = V8.
 *  det M = V0*(V8*V4 - V7*V5) - V3*(V8*V1 - V7*V2) + V6*(V5*V1 - V4*V2)
 */
int64_t csc_det_3x3_matric(int64_t * M3)
{
	int64_t M2_0[4], M2_1[4], M2_2[4];
	int64_t det0 = 0;
	int64_t det1 = 0;
	int64_t det2 = 0;

	M2_0[0] = M3[4];
	M2_0[1] = M3[5];
	M2_0[2] = M3[7];
	M2_0[3] = M3[8];
	det0 = csc_det_2x2_matric(M2_0);

	M2_1[0] = M3[1];
	M2_1[1] = M3[2];
	M2_1[2] = M3[7];
	M2_1[3] = M3[8];
	det1 = csc_det_2x2_matric(M2_1);

	M2_2[0] = M3[1];
	M2_2[1] = M3[2];
	M2_2[2] = M3[4];
	M2_2[3] = M3[5];
	det2 = csc_det_2x2_matric(M2_2);

	return ((M3[0] * det0) - (M3[3] * det1) + (M3[6] * det2));
}

/**
 *  csc_inverse_3x3_matrix
 *
 *  get the inverse of 3x3 matrix
 *
 *  note: the 3x3 matrix will be represented in a vector with 9 elements.
 *  M00 = V0, M01 = V1, M02 = V2, M10 = V3, M11 = V4, M12 = V5, M20 = V6, M21
 *  = V7, M22 = V8.
 */
int64_t csc_inverse_3x3_matrix(int64_t * M3, int64_t * M3_out)
{
	int64_t M2[4];
	int64_t det_M3 = 0;
	int64_t det[9];
	int i = 0;

	M2[0] = M3[4];
	M2[1] = M3[5];
	M2[2] = M3[7];
	M2[3] = M3[8];
	det[0] = csc_det_2x2_matric(M2);

	M2[0] = M3[2];
	M2[1] = M3[1];
	M2[2] = M3[8];
	M2[3] = M3[7];
	det[1] = csc_det_2x2_matric(M2);

	M2[0] = M3[1];
	M2[1] = M3[2];
	M2[2] = M3[4];
	M2[3] = M3[5];
	det[2] = csc_det_2x2_matric(M2);

	M2[0] = M3[5];
	M2[1] = M3[3];
	M2[2] = M3[8];
	M2[3] = M3[6];
	det[3] = csc_det_2x2_matric(M2);

	M2[0] = M3[0];
	M2[1] = M3[2];
	M2[2] = M3[6];
	M2[3] = M3[8];
	det[4] = csc_det_2x2_matric(M2);

	M2[0] = M3[2];
	M2[1] = M3[0];
	M2[2] = M3[5];
	M2[3] = M3[3];
	det[5] = csc_det_2x2_matric(M2);

	M2[0] = M3[3];
	M2[1] = M3[4];
	M2[2] = M3[6];
	M2[3] = M3[7];
	det[6] = csc_det_2x2_matric(M2);

	M2[0] = M3[1];
	M2[1] = M3[0];
	M2[2] = M3[7];
	M2[3] = M3[6];
	det[7] = csc_det_2x2_matric(M2);

	M2[0] = M3[0];
	M2[1] = M3[1];
	M2[2] = M3[3];
	M2[3] = M3[4];
	det[8] = csc_det_2x2_matric(M2);

	for (i = 0; i < 9; i++)
		M3_out[i] = det[i];

	det_M3 = csc_det_3x3_matric(M3);

	return det_M3;
}

/**
 *  csc_func1
 *
 *  csc interim function #1
 *
 */
static int64_t csc_func1(int64_t chrom1, int64_t chrom2)
{
	return csc_div64((chrom1 * MULTIPLIER_MATRIX), chrom2);
}

/**
 *  csc_func2
 *
 *  csc interim function #2
 *
 */
static int64_t csc_func2(int64_t chrom1, int64_t chrom2)
{
	return csc_div64((MULTIPLIER_CHROM - chrom1 - chrom2) *
			 MULTIPLIER_MATRIX, chrom2);
}

/**
 *  csc_func3
 *
 *  csc interim function #3
 *
 */
static int64_t csc_func3(int64_t M3_1, int64_t M3_2, int64_t M3_3,
			 int64_t chrom1, int64_t chrom2)
{
	return csc_div64(M3_1 * chrom1,
			 chrom2) + M3_2 + csc_div64(M3_3 * (MULTIPLIER_CHROM -
							    chrom1 - chrom2),
						    chrom2);
}

/**
 *  csc_func4
 *
 *  csc interim function #4
 *
 */
static int64_t csc_func4(int64_t Y1, int64_t chrom1, int64_t chrom2,
			 int64_t det1)
{
	return csc_div64(MULTIPLIER_MATRIX1 * Y1 * chrom1, chrom2 * det1);
}

/**
 *  csc_func5
 *
 *  csc interim function #5
 *
 */
static int64_t csc_func5(int64_t Y1, int64_t det1)
{
	return csc_div64(MULTIPLIER_MATRIX1 * Y1, det1);
}

/**
 *  csc_func6
 *
 *  csc interim function #6
 *
 */
static int64_t csc_func6(int64_t Y1, int64_t chrom1, int64_t chrom2,
			 int64_t det1)
{
	return csc_div64(MULTIPLIER_MATRIX1 * Y1 * (MULTIPLIER_CHROM - chrom1 -
						    chrom2), chrom2 * det1);
}

/**
 *  csc_XYZ
 *
 *  Get the transformation matrix from the input color space to CIE XYZ color
 *  space.
 *
 *  note: the input parameters are the chromaticity values. They are 10000
 *  times the actuall values.
 *  xr = chrom[0], yr = chrom[1], xg = chrom[2], yg = chrom[3], xb =
 *  chrom[4], yb = chrom[5], xw = chrom[6], yw = chrom[7].
 *
 *  See display SAS for the detailed algorithem.
 *
 */
static void csc_XYZ(int *chrom, int64_t * M_csc)
{
	int64_t M3_in[9];
	int64_t M3_out[9];
	int64_t det_M3 = 0;
	int64_t Y[3];
	int i = 0;

	/*
	 * Get the matrix to convert from RGB space to XYZ space.
	 *
	 */

	for (i = 0; i < 3; i++) {
		M3_in[i] = csc_func1(chrom[2 * i], chrom[(2 * i) + 1]);
		M3_in[i + 3] = MULTIPLIER_MATRIX;
		M3_in[i + 6] = csc_func2(chrom[2 * i], chrom[(2 * i) + 1]);
	}

	det_M3 = csc_inverse_3x3_matrix(M3_in, M3_out);
	det_M3 = csc_div64(det_M3, MULTIPLIER_MATRIX);

	for (i = 0; i < 3; i++)
		Y[i] = csc_func3(M3_out[i * 3], M3_out[(i * 3) + 1],
				 M3_out[(i * 3) + 2], chrom[6], chrom[7]);

	for (i = 0; i < 3; i++) {
		M_csc[i] =
		    csc_func4(Y[i], chrom[i * 2], chrom[(i * 2) + 1], det_M3);
		M_csc[i + 3] = csc_func5(Y[i], det_M3);
		M_csc[i + 6] =
		    csc_func6(Y[i], chrom[i * 2], chrom[(i * 2) + 1], det_M3);
	}

	for (i = 0; i < 9; i++) {
		if (M_csc[i] > 0)
			M_csc[i] = csc_div64(M_csc[i] + 5, 10);
		else
			M_csc[i] = csc_div64(M_csc[i] - 5, 10);
	}
}

/**
 *  csc_to_12bit_register_value
 *
 *  Convert the 64bit integer to a 12bit 2-complement fixed point value in
 *  format {12, 10,1}
 *
 */
static void csc_to_12bit_register_value(int64_t csc, u16 * reg_val)
{
	uint64_t temp_N = (uint64_t) - 1;
	uint64_t temp64 = 0;
	u32 temp_32_1;
	u32 temp_32_2;
	bool sign = true;	/* true: positive, false: negative. */
	u16 remain = 0;
	u8 integer = 0;

	/*
	 * Convert the signed number to absolute value.
	 *
	 */
	if (csc < 0) {
		sign = false;
		temp64 = temp_N - ((uint64_t) csc) + 1;
		temp_32_2 = (u32) temp64;
		temp_32_1 = (u32) (temp64 >> 32);
	} else {
		temp64 = (uint64_t) csc;
		temp_32_2 = (u32) temp64;
		temp_32_1 = (u32) (temp64 >> 32);
	}

	/*
	 * Convert the absolute value to register value.
	 *
	 */
	integer = temp_32_2 / 1024;
	remain = temp_32_2 % 1024;

        *reg_val = 0;
        remain = (remain * 1024) / 1024;
        *reg_val |= remain;

        if (integer)
                *reg_val |= BIT10;

        if (!sign) {
                (*reg_val) = ~(*reg_val);
                (*reg_val)++;
                (*reg_val) &= 0xFFF;
        }

        if (integer > 1)
                DRM_ERROR("Invalid parameters\n");
}

/**
 *  csc_program_DC
 *
 *  Program DC color matrix coefficients register
 *
 *  note: the 3x3 matrix will be represented in a vector with 9 elements.
 *  M00 = V0, M01 = V1, M02 = V2, M10 = V3, M11 = V4, M12 = V5, M20 = V6, M21
 *  = V7, M22 = V8.
 *
 */
void csc_program_DC(struct drm_device *dev, int64_t * csc, int pipe)
{
	u16 reg_val1 = 0, reg_val2 = 0;
	u32 reg_val = 0;
	u32 color_coef_reg = PIPEA_COLOR_COEF0;
	int i = 0;

	if (pipe == PIPEB)
		color_coef_reg += PIPEB_OFFSET;
	else if (pipe == PIPEC)
		color_coef_reg += PIPEC_OFFSET;

	/*
	 *  Convert the 64bit integer to a 12bit 2-complement fixed point value in
	 *  format {12, 10,1}
	 *
	 */
	for (i = 0; i < 9; i += 3) {
		csc_to_12bit_register_value(csc[i], &reg_val1);
		csc_to_12bit_register_value(csc[i + 1], &reg_val2);
		reg_val = reg_val1 | (reg_val2 << CC_1_POS);
		REG_WRITE(color_coef_reg, reg_val);
		color_coef_reg += 4;
		csc_to_12bit_register_value(csc[i + 2], &reg_val1);
		reg_val = reg_val1;
		REG_WRITE(color_coef_reg, reg_val);
		color_coef_reg += 4;
	}
}

/**
 *  csc_operation
 *
 *  Program DC register to perform csc.
 *
 *  note: the input parameters are the chromaticity values. They are 10000
 *  times the actuall values.
 *  xr = chrom[0], yr = chrom[1], xg = chrom[2], yg = chrom[3], xb =
 *  chrom[4], yb = chrom[5], xw = chrom[6], yw = chrom[7].
 *
 *  chrom1 represents the input color space; chrom2 represents the output
 *  color space.
 *
 *  See display SAS for the detailed algorithem.
 *
 */
void csc(struct drm_device *dev, int *chrom1, int *chrom2, int pipe)
{
	int64_t M3_out[9];
	int64_t det_M3 = 0;
	int64_t csc1[9];
	int64_t csc2[9];
	int64_t csc2_inv[9];
	int64_t csc[9];
	int i = 0;

	/*
	 * Get the matrix to convert from RGB space to XYZ space.
	 *
	 */

	csc_XYZ(chrom1, csc1);
	csc_XYZ(chrom2, csc2);

	det_M3 = csc_inverse_3x3_matrix(csc2, M3_out);
	det_M3 = csc_div64(det_M3, MULTIPLIER_MATRIX2);

	for (i = 0; i < 9; i++) {
		csc2_inv[i] =
		    csc_div64(M3_out[i] * MULTIPLIER_MATRIX1 * 1000, det_M3);

		if (csc2_inv[i] > 0)
			csc2_inv[i] = csc_div64(csc2_inv[i] + 50, 100);
		else
			csc2_inv[i] = csc_div64(csc2_inv[i] - 50, 100);
	}

	csc_matrix_mult(csc1, csc2_inv, csc);

	for (i = 0; i < 9; i++) {
		if (csc[i] > 0)
			csc[i] = csc_div64(csc[i] + 50000, 100000);
		else
			csc[i] = csc_div64(csc[i] - 50000, 100000);
	}

	csc_program_DC(dev, csc, pipe);
}
