/********************************************************
 * Kernels to be optimized for the CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "defs.h"

/*
 * Please fill in the following team struct
 */
team_t team = {
    "afkbrb", /* Team name */

    "afkbrb",           /* First member full name */
    "2w6f8c@gmail.com", /* First member email address */

    "afkbrb",          /* Second member full name (leave blank if none) */
    "2w6f8c@gmail.com" /* Second member email addr (leave blank if none) */
};

/***************
 * ROTATE KERNEL
 ***************/

/******************************************************
 * Your different versions of the rotate kernel go here
 ******************************************************/

/*
 * naive_rotate - The naive baseline version of rotate
 */
char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst) {
    int i, j;

    for (i = 0; i < dim; i++)
        for (j = 0; j < dim; j++)
            dst[RIDX(dim - 1 - j, i, dim)] = src[RIDX(i, j, dim)];
}

/**
 * https://stackoverflow.com/questions/50671488/optimization-via-loop-blocking-in-c:
 *
 * Contiguous writes and scattered reads would probably be better.
 * Evicting a dirty cache line is more expensive than evicting a clean line and
 * just re-reading it again later.
 */
char write_first_rotate_descr[] =
    "write_first_rotate: write first implementation";
void write_first_rotate(int dim, pixel *src, pixel *dst) {
    int i, j;

    for (i = 0; i < dim; i++)
        for (j = 0; j < dim; j++)
            dst[RIDX(dim - 1 - i, j, dim)] = src[RIDX(j, i, dim)];
}

char block_32_rotate_descr[] = "block_32_rotate: block 32 implementation";
void block_32_rotate(int dim, pixel *src, pixel *dst) {
    int block = 32;
    for (int i = 0; i < dim; i += block) {
        for (int j = 0; j < dim; j += block) {
            for (int ii = i; ii < i + block; ii++) {
                for (int jj = j; jj < j + block; jj++) {
                    dst[RIDX(dim - 1 - ii, jj, dim)] = src[RIDX(jj, ii, dim)];
                }
            }
        }
    }
}

char block_16_rotate_descr[] = "block_16_rotate: block 16 implementation";
void block_16_rotate(int dim, pixel *src, pixel *dst) {
    int block = 16;
    for (int i = 0; i < dim; i += block) {
        for (int j = 0; j < dim; j += block) {
            for (int ii = i; ii < i + block; ii++) {
                for (int jj = j; jj < j + block; jj++) {
                    dst[RIDX(dim - 1 - ii, jj, dim)] = src[RIDX(jj, ii, dim)];
                }
            }
        }
    }
}

char block_16_unrolling_rotate_descr[] =
    "block_16_unrolling_rotate: block 16 unrolling implementation";
void block_16_unrolling_rotate(int dim, pixel *src, pixel *dst) {
    int block = 16;
    for (int i = 0; i < dim; i += block) {
        for (int j = 0; j < dim; j += block) {
            for (int ii = i; ii < i + block; ii++) {
                int base = (dim - 1 - ii) * dim;
                dst[base + j] = src[RIDX(j, ii, dim)];
                dst[base + j + 1] = src[RIDX(j + 1, ii, dim)];
                dst[base + j + 2] = src[RIDX(j + 2, ii, dim)];
                dst[base + j + 3] = src[RIDX(j + 3, ii, dim)];
                dst[base + j + 4] = src[RIDX(j + 4, ii, dim)];
                dst[base + j + 5] = src[RIDX(j + 5, ii, dim)];
                dst[base + j + 6] = src[RIDX(j + 6, ii, dim)];
                dst[base + j + 7] = src[RIDX(j + 7, ii, dim)];
                dst[base + j + 8] = src[RIDX(j + 8, ii, dim)];
                dst[base + j + 9] = src[RIDX(j + 9, ii, dim)];
                dst[base + j + 10] = src[RIDX(j + 10, ii, dim)];
                dst[base + j + 11] = src[RIDX(j + 11, ii, dim)];
                dst[base + j + 12] = src[RIDX(j + 12, ii, dim)];
                dst[base + j + 13] = src[RIDX(j + 13, ii, dim)];
                dst[base + j + 14] = src[RIDX(j + 14, ii, dim)];
                dst[base + j + 15] = src[RIDX(j + 15, ii, dim)];
            }
        }
    }
}

/*
 * rotate - Your current working version of rotate
 * IMPORTANT: This is the version you will be graded on
 */
char rotate_descr[] = "rotate: Current working version";
void rotate(int dim, pixel *src, pixel *dst) {
    block_16_unrolling_rotate(dim, src, dst);
}

/*********************************************************************
 * register_rotate_functions - Register all of your different versions
 *     of the rotate kernel with the driver by calling the
 *     add_rotate_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.
 *********************************************************************/

void register_rotate_functions() {
    add_rotate_function(&naive_rotate, naive_rotate_descr);
    add_rotate_function(&write_first_rotate, write_first_rotate_descr);
    add_rotate_function(&block_32_rotate, block_32_rotate_descr);
    add_rotate_function(&block_16_rotate, block_16_rotate_descr);
    add_rotate_function(&block_16_unrolling_rotate,
                        block_16_unrolling_rotate_descr);
    add_rotate_function(&rotate, rotate_descr);
    /* ... Register additional test functions here */
}

/***************
 * SMOOTH KERNEL
 **************/

/***************************************************************
 * Various typedefs and helper functions for the smooth function
 * You may modify these any way you like.
 **************************************************************/

/* A struct used to compute averaged pixel value */
typedef struct {
    int red;
    int green;
    int blue;
    int num;
} pixel_sum;

/* Compute min and max of two integers, respectively */
static int min(int a, int b) { return (a < b ? a : b); }
static int max(int a, int b) { return (a > b ? a : b); }

/*
 * initialize_pixel_sum - Initializes all fields of sum to 0
 */
static void initialize_pixel_sum(pixel_sum *sum) {
    sum->red = sum->green = sum->blue = 0;
    sum->num = 0;
    return;
}

/*
 * accumulate_sum - Accumulates field values of p in corresponding
 * fields of sum
 */
static void accumulate_sum(pixel_sum *sum, pixel p) {
    sum->red += (int)p.red;
    sum->green += (int)p.green;
    sum->blue += (int)p.blue;
    sum->num++;
    return;
}

/*
 * assign_sum_to_pixel - Computes averaged pixel value in current_pixel
 */
static void assign_sum_to_pixel(pixel *current_pixel, pixel_sum sum) {
    current_pixel->red = (unsigned short)(sum.red / sum.num);
    current_pixel->green = (unsigned short)(sum.green / sum.num);
    current_pixel->blue = (unsigned short)(sum.blue / sum.num);
    return;
}

/*
 * avg - Returns averaged pixel value at (i,j)
 */
static pixel avg(int dim, int i, int j, pixel *src) {
    int ii, jj;
    pixel_sum sum;
    pixel current_pixel;

    initialize_pixel_sum(&sum);
    for (ii = max(i - 1, 0); ii <= min(i + 1, dim - 1); ii++)
        for (jj = max(j - 1, 0); jj <= min(j + 1, dim - 1); jj++)
            accumulate_sum(&sum, src[RIDX(ii, jj, dim)]);

    assign_sum_to_pixel(&current_pixel, sum);
    return current_pixel;
}

/******************************************************
 * Your different versions of the smooth kernel go here
 ******************************************************/

/*
 * naive_smooth - The naive baseline version of smooth
 */
char naive_smooth_descr[] = "naive_smooth: Naive baseline implementation";
void naive_smooth(int dim, pixel *src, pixel *dst) {
    int i, j;

    for (i = 0; i < dim; i++)
        for (j = 0; j < dim; j++) dst[RIDX(i, j, dim)] = avg(dim, i, j, src);
}

/*
 * smooth - Your current working version of smooth.
 * IMPORTANT: This is the version you will be graded on
 */
char smooth_descr[] = "smooth: Current working version";
void smooth(int dim, pixel *src, pixel *dst) {
    for (int i = 0; i < dim; i++) {
        dst[i] = avg(dim, 0, i, src);
        dst[RIDX(dim - 1, i, dim)] = avg(dim, dim - 1, i, src);
        dst[RIDX(i, 0, dim)] = avg(dim, i, 0, src);
        dst[RIDX(i, dim - 1, dim)] = avg(dim, i, dim - 1, src);
    }

    for (int i = 1; i < dim - 1; i++) {
        for (int j = 1; j < dim - 1; j++) {
            int r = 0, g = 0, b = 0;
            pixel p1 = src[RIDX(i - 1, j - 1, dim)];
            pixel p2 = src[RIDX(i - 1, j, dim)];
            pixel p3 = src[RIDX(i - 1, j + 1, dim)];
            pixel p4 = src[RIDX(i, j - 1, dim)];
            pixel p5 = src[RIDX(i, j, dim)];
            pixel p6 = src[RIDX(i, j + 1, dim)];
            pixel p7 = src[RIDX(i + 1, j - 1, dim)];
            pixel p8 = src[RIDX(i + 1, j, dim)];
            pixel p9 = src[RIDX(i + 1, j + 1, dim)];

            r = p1.red + p2.red + p3.red + p4.red + p5.red + p6.red + p7.red +
                p8.red + p9.red;
            g = p1.green + p2.green + p3.green + p4.green + p5.green +
                p6.green + p7.green + p8.green + p9.green;
            b = p1.blue + p2.blue + p3.blue + p4.blue + p5.blue + p6.blue +
                p7.blue + p8.blue + p9.blue;

            pixel *t = dst + i * dim + j;
            t->red = r / 9;
            t->green = g / 9;
            t->blue = b / 9;
        }
    }
}

/*********************************************************************
 * register_smooth_functions - Register all of your different versions
 *     of the smooth kernel with the driver by calling the
 *     add_smooth_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.
 *********************************************************************/

void register_smooth_functions() {
    add_smooth_function(&smooth, smooth_descr);
    add_smooth_function(&naive_smooth, naive_smooth_descr);
    /* ... Register additional test functions here */
}
