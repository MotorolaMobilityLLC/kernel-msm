/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*===========================================================================

                     radar_filters.h

  OVERVIEW:

  Source code borrowed from QCA_MAIN DFS module

  DEPENDENCIES:

  Are listed for each API below.

===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.



  when        who     what, where, why
----------    ---    --------------------------------------------------------

===========================================================================*/





struct dfs_pulse dfs_fcc_radars[] = {
    // FCC TYPE 1
    // {18,  1,  325, 1930, 0,  6,  7,  0,  1, 18,  0, 3,  0}, // 518 to 3066
    {18,  1,  700, 700, 0,  6,  5,  0,  1, 18,  0, 3,  1, 0},
    {18,  1,  350, 350, 0,  6,  5,  0,  1, 18,  0, 3,  0, 0},

    // FCC TYPE 6
    // {9,   1, 3003, 3003, 1,  7,  5,  0,  1, 18,  0, 0,  1}, // 333 +/- 7 us
    {9,   1, 3003, 3003, 1,  7,  5,  0,  1, 18,  0, 0,  1, 1},

    // FCC TYPE 2
    {23, 5, 4347, 6666, 0, 18, 8,  0,  7, 22,  0, 3,  0, 2},

    // FCC TYPE 3
    {18, 10, 2000, 5000, 0, 23,  6,  6, 13, 22,  0, 3, 0, 5},

    // FCC TYPE 4
    {16, 15, 2000, 5000, 0, 25,  5, 11, 23, 22,  0, 3, 0, 11},
};

struct dfs_pulse dfs_mkk4_radars[] = {
    /* following two filters are specific to Japan/MKK4 */
//    {18,  1,  720,  720, 1,  6,  6,  0,  1, 18,  0, 3, 17}, // 1389 +/- 6 us
//    {18,  4,  250,  250, 1, 10,  5,  1,  6, 18,  0, 3, 18}, // 4000 +/- 6 us
//    {18,  5,  260,  260, 1, 10,  6,  1,  6, 18,  0, 3, 19}, // 3846 +/- 7 us
    {18,  1,  720,  720, 0,  6,  6,  0,  1, 18,  0, 3, 0, 17}, // 1389 +/- 6 us
    {18,  4,  250,  250, 0, 10,  5,  1,  6, 18,  0, 3, 0, 18}, // 4000 +/- 6 us
    {18,  5,  260,  260, 0, 10,  6,  1,  6, 18,  0, 3, 1, 19}, // 3846 +/- 7 us

    /* following filters are common to both FCC and JAPAN */

    // FCC TYPE 1
    // {18,  1,  325, 1930, 0,  6,  7,  0,  1, 18,  0, 3,  0}, // 518 to 3066
    {18,  1,  700, 700, 0,  6,  5,  0,  1, 18,  0, 3,  1, 0},
    {18,  1,  350, 350, 0,  6,  5,  0,  1, 18,  0, 3,  0, 0},

    // FCC TYPE 6
    // {9,   1, 3003, 3003, 1,  7,  5,  0,  1, 18,  0, 0,  1}, // 333 +/- 7 us
    {9,   1, 3003, 3003, 1,  7,  5,  0,  1, 18,  0, 0, 1,  1},

    // FCC TYPE 2
    {23, 5, 4347, 6666, 0, 18, 8,  0,  7, 22,  0, 3,  0, 2},

    // FCC TYPE 3
    {18, 10, 2000, 5000, 0, 23,  6,  6, 13, 22,  0, 3, 0, 5},

    // FCC TYPE 4
    {16, 15, 2000, 5000, 0, 25,  5, 11, 23, 22,  0, 3, 0, 11},
};

struct dfs_bin5pulse dfs_fcc_bin5pulses[] = {
        {4, 28, 105, 12, 22, 5},
};

struct dfs_bin5pulse dfs_jpn_bin5pulses[] = {
        {5, 28, 105, 12, 22, 5},
};
struct dfs_pulse dfs_etsi_radars[] = {

    /* TYPE staggered pulse */
    /* 0.8-2us, 2-3 bursts,300-400 PRF, 10 pulses each */
    {30,  2,  300,  400, 2, 30,  3,  0,  5, 15, 0,   0, 1, 31},   /* Type 5*/
    /* 0.8-2us, 2-3 bursts, 400-1200 PRF, 15 pulses each */
    {30,  2,  400, 1200, 2, 30,  7,  0,  5, 15, 0,   0, 0, 32},   /* Type 6 */

    /* constant PRF based */
    /* 0.8-5us, 200  300 PRF, 10 pulses */
    {10, 5,   200,  400, 0, 24,  5,  0,  8, 15, 0,   0, 2, 33},   /* Type 1 */
    {10, 5,   400,  600, 0, 24,  5,  0,  8, 15, 0,   0, 2, 37},   /* Type 1 */
    {10, 5,   600,  800, 0, 24,  5,  0,  8, 15, 0,   0, 2, 38},   /* Type 1 */
    {10, 5,   800, 1000, 0, 24,  5,  0,  8, 15, 0,   0, 2, 39},   /* Type 1 */
//  {10, 5,   200, 1000, 0, 24,  5,  0,  8, 15, 0,   0, 2, 33},

    /* 0.8-15us, 200-1600 PRF, 15 pulses */
    {15, 15,  200, 1600, 0, 24, 8,  0, 18, 24, 0,   0, 0, 34},    /* Type 2 */

    /* 0.8-15us, 2300-4000 PRF, 25 pulses*/
    {25, 15, 2300, 4000,  0, 24, 10, 0, 18, 24, 0,   0, 0, 35},   /* Type 3 */

    /* 20-30us, 2000-4000 PRF, 20 pulses*/
    {20, 30, 2000, 4000, 0, 24, 6, 19, 33, 24, 0,   0, 0, 36},    /* Type 4 */
};
