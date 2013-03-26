/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/leds.h>
#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_NT35590.h"

static struct dsi_buf nt35590_tx_buf;
static struct dsi_buf nt35590_rx_buf;

static int wled_trigger_initialized;

static char display_off[2] = {0x28, 0x00};
static char enter_sleep[2] = {0x10, 0x00};

static struct dsi_cmd_desc nt35590_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 50, sizeof(enter_sleep), enter_sleep}
};

static char disp_on0[2] = {0xFF, 0xEE};
static char disp_on1[2] = {0x26, 0x08};
static char disp_on2[2] = {0x26, 0x00};
static char disp_on3[2] = {0xFF, 0x00}; /* WaitTime(10) */

static char lane[2] = {0xBA, 0x03}; /* MIPI 4 lane */
/* Fix 180 degree orientation */
/* static char disp_on_rotate[2] = {0x36, 0xd7}; */

static char command_mode[2] = {0xC2, 0x08}; /* Setting 0x08 for MIPI cmd mode */

static char video_mode[2] = {0xC2, 0x03}; /* Setting 0x03 for MIPI video mode */

static char disp_on6[2] = {0xFF, 0x01}; /* CMD page select */
static char disp_on7[2] = {0xFB, 0x01}; /* RELOAD CMD1 */
static char disp_on8[2] = {0x00, 0x4A};
static char disp_on9[2] = {0x01, 0x33};
static char disp_on10[2] = {0x02, 0x53};
static char disp_on11[2] = {0x03, 0x55};
static char disp_on12[2] = {0x04, 0x55};
static char disp_on13[2] = {0x05, 0x33};
static char disp_on14[2] = {0x06, 0x22};
static char disp_on15[2] = {0x08, 0x56};
static char disp_on16[2] = {0x09, 0x8F};
static char disp_on17[2] = {0x36, 0x73};
static char disp_on18[2] = {0x0B, 0x9F};
static char disp_on19[2] = {0x0C, 0x9F};
static char disp_on20[2] = {0x0D, 0x2F};
static char disp_on21[2] = {0x0E, 0x24};
static char disp_on22[2] = {0x11, 0x83};
static char disp_on23[2] = {0x12, 0x03};
static char disp_on24[2] = {0x71, 0x2C};
static char disp_on25[2] = {0x6F, 0x03};
static char disp_on26[2] = {0x0F, 0x0A};
static char disp_on27[2] = {0xFF, 0x05}; /* CMD page select */
static char disp_on28[2] = {0xFB, 0x01}; /* RELOAD CMD1 */
static char disp_on29[2] = {0x01, 0x00};
static char disp_on30[2] = {0x02, 0x8B}; /* 60 TE signals from panel */
static char disp_on31[2] = {0x03, 0x82};
static char disp_on32[2] = {0x04, 0x82};
static char disp_on33[2] = {0x05, 0x30};
static char disp_on34[2] = {0x06, 0x33};
static char disp_on35[2] = {0x07, 0x01};
static char disp_on36[2] = {0x08, 0x00};
static char disp_on37[2] = {0x09, 0x46};
static char disp_on38[2] = {0x0A, 0x46};
static char disp_on39[2] = {0x0D, 0x0B};
static char disp_on40[2] = {0x0E, 0x1D};
static char disp_on41[2] = {0x0F, 0x08};
static char disp_on42[2] = {0x10, 0x53};
static char disp_on43[2] = {0x11, 0x00};
static char disp_on44[2] = {0x12, 0x00};
static char disp_on45[2] = {0x14, 0x01};
static char disp_on46[2] = {0x15, 0x00};
static char disp_on47[2] = {0x16, 0x05};
static char disp_on48[2] = {0x17, 0x00};
static char disp_on49[2] = {0x19, 0x7F};
static char disp_on50[2] = {0x1A, 0xFF};
static char disp_on51[2] = {0x1B, 0x0F};
static char disp_on52[2] = {0x1C, 0x00};
static char disp_on53[2] = {0x1D, 0x00};
static char disp_on54[2] = {0x1E, 0x00};
static char disp_on55[2] = {0x1F, 0x07};
static char disp_on56[2] = {0x20, 0x00};
static char disp_on57[2] = {0x21, 0x06};
static char disp_on58[2] = {0x22, 0x55};
static char disp_on59[2] = {0x23, 0x4D};
static char disp_on60[2] = {0x2D, 0x02};
static char disp_on61[2] = {0x28, 0x01};
static char disp_on62[2] = {0x2F, 0x02};
static char disp_on63[2] = {0x83, 0x01};
static char disp_on64[2] = {0x9E, 0x58};
static char disp_on65[2] = {0x9F, 0x6A};
static char disp_on66[2] = {0xA0, 0x01};
static char disp_on67[2] = {0xA2, 0x10};
static char disp_on68[2] = {0xBB, 0x0A};
static char disp_on69[2] = {0xBC, 0x0A};
static char disp_on70[2] = {0x32, 0x08};
static char disp_on71[2] = {0x33, 0xB8};
static char disp_on72[2] = {0x36, 0x01};
static char disp_on73[2] = {0x37, 0x00};
static char disp_on74[2] = {0x43, 0x00};
static char disp_on75[2] = {0x4B, 0x21};
static char disp_on76[2] = {0x4C, 0x03};
static char disp_on77[2] = {0x50, 0x21};
static char disp_on78[2] = {0x51, 0x03};
static char disp_on79[2] = {0x58, 0x21};
static char disp_on80[2] = {0x59, 0x03};
static char disp_on81[2] = {0x5D, 0x21};
static char disp_on82[2] = {0x5E, 0x03};
static char disp_on83[2] = {0x6C, 0x00};
static char disp_on84[2] = {0x6D, 0x00};
static char disp_on85[2] = {0xFB, 0x01};
static char disp_on86[2] = {0xFF, 0x01}; /* Enter CMD2,Page0 */
static char disp_on87[2] = {0xFB, 0x01};
/* Gamma+ R settings start */
static char disp_on88[2] = {0x75, 0x00};
static char disp_on89[2] = {0x76, 0x7D};
static char disp_on90[2] = {0x77, 0x00};
static char disp_on91[2] = {0x78, 0x8A};
static char disp_on92[2] = {0x79, 0x00};
static char disp_on93[2] = {0x7A, 0x9C};
static char disp_on94[2] = {0x7B, 0x00};
static char disp_on95[2] = {0x7C, 0xB1};
static char disp_on96[2] = {0x7D, 0x00};
static char disp_on97[2] = {0x7E, 0xBF};
static char disp_on98[2] = {0x7F, 0x00};
static char disp_on99[2] = {0x80, 0xCF};
static char disp_on100[2] = {0x81, 0x00};
static char disp_on101[2] = {0x82, 0xDD};
static char disp_on102[2] = {0x83, 0x00};
static char disp_on103[2] = {0x84, 0xE8};
static char disp_on104[2] = {0x85, 0x00};
static char disp_on105[2] = {0x86, 0xF2};
static char disp_on106[2] = {0x87, 0x01};
static char disp_on107[2] = {0x88, 0x1F};
static char disp_on108[2] = {0x89, 0x01};
static char disp_on109[2] = {0x8A, 0x41};
static char disp_on110[2] = {0x8B, 0x01};
static char disp_on111[2] = {0x8C, 0x78};
static char disp_on112[2] = {0x8D, 0x01};
static char disp_on113[2] = {0x8E, 0xA5};
static char disp_on114[2] = {0x8F, 0x01};
static char disp_on115[2] = {0x90, 0xEE};
static char disp_on116[2] = {0x91, 0x02};
static char disp_on117[2] = {0x92, 0x29};
static char disp_on118[2] = {0x93, 0x02};
static char disp_on119[2] = {0x94, 0x2A};
static char disp_on120[2] = {0x95, 0x02};
static char disp_on121[2] = {0x96, 0x5D};
static char disp_on122[2] = {0x97, 0x02};
static char disp_on123[2] = {0x98, 0x93};
static char disp_on124[2] = {0x99, 0x02};
static char disp_on125[2] = {0x9A, 0xB8};
static char disp_on126[2] = {0x9B, 0x02};
static char disp_on127[2] = {0x9C, 0xE7};
static char disp_on128[2] = {0x9D, 0x03};
static char disp_on129[2] = {0x9E, 0x07};
static char disp_on130[2] = {0x9F, 0x03};
static char disp_on131[2] = {0xA0, 0x37};
static char disp_on132[2] = {0xA2, 0x03};
static char disp_on133[2] = {0xA3, 0x46};
static char disp_on134[2] = {0xA4, 0x03};
static char disp_on135[2] = {0xA5, 0x56};
static char disp_on136[2] = {0xA6, 0x03};
static char disp_on137[2] = {0xA7, 0x66};
static char disp_on138[2] = {0xA9, 0x03};
static char disp_on139[2] = {0xAA, 0x7A};
static char disp_on140[2] = {0xAB, 0x03};
static char disp_on141[2] = {0xAC, 0x93};
static char disp_on142[2] = {0xAD, 0x03};
static char disp_on143[2] = {0xAE, 0xA3};
static char disp_on144[2] = {0xAF, 0x03};
static char disp_on145[2] = {0xB0, 0xB4};
static char disp_on146[2] = {0xB1, 0x03};
static char disp_on147[2] = {0xB2, 0xCB};
/* Gamma+ R settings end */
/* Gamma- R settings start */
static char disp_on148[2] = {0xB3, 0x00};
static char disp_on149[2] = {0xB4, 0x7D};
static char disp_on150[2] = {0xB5, 0x00};
static char disp_on151[2] = {0xB6, 0x8A};
static char disp_on152[2] = {0xB7, 0x00};
static char disp_on153[2] = {0xB8, 0x9C};
static char disp_on154[2] = {0xB9, 0x00};
static char disp_on155[2] = {0xBA, 0xB1};
static char disp_on156[2] = {0xBB, 0x00};
static char disp_on157[2] = {0xBC, 0xBF};
static char disp_on158[2] = {0xBD, 0x00};
static char disp_on159[2] = {0xBE, 0xCF};
static char disp_on160[2] = {0xBF, 0x00};
static char disp_on161[2] = {0xC0, 0xDD};
static char disp_on162[2] = {0xC1, 0x00};
static char disp_on163[2] = {0xC2, 0xE8};
static char disp_on164[2] = {0xC3, 0x00};
static char disp_on165[2] = {0xC4, 0xF2};
static char disp_on166[2] = {0xC5, 0x01};
static char disp_on167[2] = {0xC6, 0x1F};
static char disp_on168[2] = {0xC7, 0x01};
static char disp_on169[2] = {0xC8, 0x41};
static char disp_on170[2] = {0xC9, 0x01};
static char disp_on171[2] = {0xCA, 0x78};
static char disp_on172[2] = {0xCB, 0x01};
static char disp_on173[2] = {0xCC, 0xA5};
static char disp_on174[2] = {0xCD, 0x01};
static char disp_on175[2] = {0xCE, 0xEE};
static char disp_on176[2] = {0xCF, 0x02};
static char disp_on177[2] = {0xD0, 0x29};
static char disp_on178[2] = {0xD1, 0x02};
static char disp_on179[2] = {0xD2, 0x2A};
static char disp_on180[2] = {0xD3, 0x02};
static char disp_on181[2] = {0xD4, 0x5D};
static char disp_on182[2] = {0xD5, 0x02};
static char disp_on183[2] = {0xD6, 0x93};
static char disp_on184[2] = {0xD7, 0x02};
static char disp_on185[2] = {0xD8, 0xB8};
static char disp_on186[2] = {0xD9, 0x02};
static char disp_on187[2] = {0xDA, 0xE7};
static char disp_on188[2] = {0xDB, 0x03};
static char disp_on189[2] = {0xDC, 0x07};
static char disp_on190[2] = {0xDD, 0x03};
static char disp_on191[2] = {0xDE, 0x37};
static char disp_on192[2] = {0xDF, 0x03};
static char disp_on193[2] = {0xE0, 0x46};
static char disp_on194[2] = {0xE1, 0x03};
static char disp_on195[2] = {0xE2, 0x56};
static char disp_on196[2] = {0xE3, 0x03};
static char disp_on197[2] = {0xE4, 0x66};
static char disp_on198[2] = {0xE5, 0x03};
static char disp_on199[2] = {0xE6, 0x7A};
static char disp_on200[2] = {0xE7, 0x03};
static char disp_on201[2] = {0xE8, 0x93};
static char disp_on202[2] = {0xE9, 0x03};
static char disp_on203[2] = {0xEA, 0xA3};
static char disp_on204[2] = {0xEB, 0x03};
static char disp_on205[2] = {0xEC, 0xB4};
static char disp_on206[2] = {0xED, 0x03};
static char disp_on207[2] = {0xEE, 0xCB};
/* Gamma- R settings end */
/* Gamma+ G settings start */
static char disp_on208[2] = {0xEF, 0x00};
static char disp_on209[2] = {0xF0, 0xED};
static char disp_on210[2] = {0xF1, 0x00};
static char disp_on211[2] = {0xF2, 0xF3};
static char disp_on212[2] = {0xF3, 0x00};
static char disp_on213[2] = {0xF4, 0xFE};
static char disp_on214[2] = {0xF5, 0x01};
static char disp_on215[2] = {0xF6, 0x09};
static char disp_on216[2] = {0xF7, 0x01};
static char disp_on217[2] = {0xF8, 0x13};
static char disp_on218[2] = {0xF9, 0x01};
static char disp_on219[2] = {0xFA, 0x1D};
/* Gamma+ G settings end */
static char disp_on220[2] = {0xFF, 0x02}; /* Enter CMD */
static char disp_on221[2] = {0xFB, 0x01};
/* Gamma+ G settings start */
static char disp_on222[2] = {0x00, 0x01};
static char disp_on223[2] = {0x01, 0x26};
static char disp_on224[2] = {0x02, 0x01};
static char disp_on225[2] = {0x03, 0x2F};
static char disp_on226[2] = {0x04, 0x01};
static char disp_on227[2] = {0x05, 0x37};
static char disp_on228[2] = {0x06, 0x01};
static char disp_on229[2] = {0x07, 0x56};
static char disp_on230[2] = {0x08, 0x01};
static char disp_on231[2] = {0x09, 0x70};
static char disp_on232[2] = {0x0A, 0x01};
static char disp_on233[2] = {0x0B, 0x9D};
static char disp_on234[2] = {0x0C, 0x01};
static char disp_on235[2] = {0x0D, 0xC2};
static char disp_on236[2] = {0x0E, 0x01};
static char disp_on237[2] = {0x0F, 0xFF};
static char disp_on238[2] = {0x10, 0x02};
static char disp_on239[2] = {0x11, 0x31};
static char disp_on240[2] = {0x12, 0x02};
static char disp_on241[2] = {0x13, 0x32};
static char disp_on242[2] = {0x14, 0x02};
static char disp_on243[2] = {0x15, 0x60};
static char disp_on244[2] = {0x16, 0x02};
static char disp_on245[2] = {0x17, 0x94};
static char disp_on246[2] = {0x18, 0x02};
static char disp_on247[2] = {0x19, 0xB5};
static char disp_on248[2] = {0x1A, 0x02};
static char disp_on249[2] = {0x1B, 0xE3};
static char disp_on250[2] = {0x1C, 0x03};
static char disp_on251[2] = {0x1D, 0x03};
static char disp_on252[2] = {0x1E, 0x03};
static char disp_on253[2] = {0x1F, 0x2D};
static char disp_on254[2] = {0x20, 0x03};
static char disp_on255[2] = {0x21, 0x3A};
static char disp_on256[2] = {0x22, 0x03};
static char disp_on257[2] = {0x23, 0x48};
static char disp_on258[2] = {0x24, 0x03};
static char disp_on259[2] = {0x25, 0x57};
static char disp_on260[2] = {0x26, 0x03};
static char disp_on261[2] = {0x27, 0x68};
static char disp_on262[2] = {0x28, 0x03};
static char disp_on263[2] = {0x29, 0x7B};
static char disp_on264[2] = {0x2A, 0x03};
static char disp_on265[2] = {0x2B, 0x90};
static char disp_on266[2] = {0x2D, 0x03};
static char disp_on267[2] = {0x2F, 0xA0};
static char disp_on268[2] = {0x30, 0x03};
static char disp_on269[2] = {0x31, 0xCB};
/* Gamma+ G settings end */
/* Gamma- G settings start */
static char disp_on270[2] = {0x32, 0x00};
static char disp_on271[2] = {0x33, 0xED};
static char disp_on272[2] = {0x34, 0x00};
static char disp_on273[2] = {0x35, 0xF3};
static char disp_on274[2] = {0x36, 0x00};
static char disp_on275[2] = {0x37, 0xFE};
static char disp_on276[2] = {0x38, 0x01};
static char disp_on277[2] = {0x39, 0x09};
static char disp_on278[2] = {0x3A, 0x01};
static char disp_on279[2] = {0x3B, 0x13};
static char disp_on280[2] = {0x3D, 0x01};
static char disp_on281[2] = {0x3F, 0x1D};
static char disp_on282[2] = {0x40, 0x01};
static char disp_on283[2] = {0x41, 0x26};
static char disp_on284[2] = {0x42, 0x01};
static char disp_on285[2] = {0x43, 0x2F};
static char disp_on286[2] = {0x44, 0x01};
static char disp_on287[2] = {0x45, 0x37};
static char disp_on288[2] = {0x46, 0x01};
static char disp_on289[2] = {0x47, 0x56};
static char disp_on290[2] = {0x48, 0x01};
static char disp_on291[2] = {0x49, 0x70};
static char disp_on292[2] = {0x4A, 0x01};
static char disp_on293[2] = {0x4B, 0x9D};
static char disp_on294[2] = {0x4C, 0x01};
static char disp_on295[2] = {0x4D, 0xC2};
static char disp_on296[2] = {0x4E, 0x01};
static char disp_on297[2] = {0x4F, 0xFF};
static char disp_on298[2] = {0x50, 0x02};
static char disp_on299[2] = {0x51, 0x31};
static char disp_on300[2] = {0x52, 0x02};
static char disp_on301[2] = {0x53, 0x32};
static char disp_on302[2] = {0x54, 0x02};
static char disp_on303[2] = {0x55, 0x60};
static char disp_on304[2] = {0x56, 0x02};
static char disp_on305[2] = {0x58, 0x94};
static char disp_on306[2] = {0x59, 0x02};
static char disp_on307[2] = {0x5A, 0xB5};
static char disp_on308[2] = {0x5B, 0x02};
static char disp_on309[2] = {0x5C, 0xE3};
static char disp_on310[2] = {0x5D, 0x03};
static char disp_on311[2] = {0x5E, 0x03};
static char disp_on312[2] = {0x5F, 0x03};
static char disp_on313[2] = {0x60, 0x2D};
static char disp_on314[2] = {0x61, 0x03};
static char disp_on315[2] = {0x62, 0x3A};
static char disp_on316[2] = {0x63, 0x03};
static char disp_on317[2] = {0x64, 0x48};
static char disp_on318[2] = {0x65, 0x03};
static char disp_on319[2] = {0x66, 0x57};
static char disp_on320[2] = {0x67, 0x03};
static char disp_on321[2] = {0x68, 0x68};
static char disp_on322[2] = {0x69, 0x03};
static char disp_on323[2] = {0x6A, 0x7B};
static char disp_on324[2] = {0x6B, 0x03};
static char disp_on325[2] = {0x6C, 0x90};
static char disp_on326[2] = {0x6D, 0x03};
static char disp_on327[2] = {0x6E, 0xA0};
static char disp_on328[2] = {0x6F, 0x03};
static char disp_on329[2] = {0x70, 0xCB};
/* Gamma- G settings end */
/* Gamma+ B settings start */
static char disp_on330[2] = {0x71, 0x00};
static char disp_on331[2] = {0x72, 0x19};
static char disp_on332[2] = {0x73, 0x00};
static char disp_on333[2] = {0x74, 0x36};
static char disp_on334[2] = {0x75, 0x00};
static char disp_on335[2] = {0x76, 0x55};
static char disp_on336[2] = {0x77, 0x00};
static char disp_on337[2] = {0x78, 0x70};
static char disp_on338[2] = {0x79, 0x00};
static char disp_on339[2] = {0x7A, 0x83};
static char disp_on340[2] = {0x7B, 0x00};
static char disp_on341[2] = {0x7C, 0x99};
static char disp_on342[2] = {0x7D, 0x00};
static char disp_on343[2] = {0x7E, 0xA8};
static char disp_on344[2] = {0x7F, 0x00};
static char disp_on345[2] = {0x80, 0xB7};
static char disp_on346[2] = {0x81, 0x00};
static char disp_on347[2] = {0x82, 0xC5};
static char disp_on348[2] = {0x83, 0x00};
static char disp_on349[2] = {0x84, 0xF7};
static char disp_on350[2] = {0x85, 0x01};
static char disp_on351[2] = {0x86, 0x1E};
static char disp_on352[2] = {0x87, 0x01};
static char disp_on353[2] = {0x88, 0x60};
static char disp_on354[2] = {0x89, 0x01};
static char disp_on355[2] = {0x8A, 0x95};
static char disp_on356[2] = {0x8B, 0x01};
static char disp_on357[2] = {0x8C, 0xE1};
static char disp_on358[2] = {0x8D, 0x02};
static char disp_on359[2] = {0x8E, 0x20};
static char disp_on360[2] = {0x8F, 0x02};
static char disp_on361[2] = {0x90, 0x23};
static char disp_on362[2] = {0x91, 0x02};
static char disp_on363[2] = {0x92, 0x59};
static char disp_on364[2] = {0x93, 0x02};
static char disp_on365[2] = {0x94, 0x94};
static char disp_on366[2] = {0x95, 0x02};
static char disp_on367[2] = {0x96, 0xB4};
static char disp_on368[2] = {0x97, 0x02};
static char disp_on369[2] = {0x98, 0xE1};
static char disp_on370[2] = {0x99, 0x03};
static char disp_on371[2] = {0x9A, 0x01};
static char disp_on372[2] = {0x9B, 0x03};
static char disp_on373[2] = {0x9C, 0x28};
static char disp_on374[2] = {0x9D, 0x03};
static char disp_on375[2] = {0x9E, 0x30};
static char disp_on376[2] = {0x9F, 0x03};
static char disp_on377[2] = {0xA0, 0x37};
static char disp_on378[2] = {0xA2, 0x03};
static char disp_on379[2] = {0xA3, 0x3B};
static char disp_on380[2] = {0xA4, 0x03};
static char disp_on381[2] = {0xA5, 0x40};
static char disp_on382[2] = {0xA6, 0x03};
static char disp_on383[2] = {0xA7, 0x50};
static char disp_on384[2] = {0xA9, 0x03};
static char disp_on385[2] = {0xAA, 0x6D};
static char disp_on386[2] = {0xAB, 0x03};
static char disp_on387[2] = {0xAC, 0x80};
static char disp_on388[2] = {0xAD, 0x03};
static char disp_on389[2] = {0xAE, 0xCB};
/* Gamma+ B settings end */
/* Gamma- B settings start */
static char disp_on390[2] = {0xAF, 0x00};
static char disp_on391[2] = {0xB0, 0x19};
static char disp_on392[2] = {0xB1, 0x00};
static char disp_on393[2] = {0xB2, 0x36};
static char disp_on394[2] = {0xB3, 0x00};
static char disp_on395[2] = {0xB4, 0x55};
static char disp_on396[2] = {0xB5, 0x00};
static char disp_on397[2] = {0xB6, 0x70};
static char disp_on398[2] = {0xB7, 0x00};
static char disp_on399[2] = {0xB8, 0x83};
static char disp_on400[2] = {0xB9, 0x00};
static char disp_on401[2] = {0xBA, 0x99};
static char disp_on402[2] = {0xBB, 0x00};
static char disp_on403[2] = {0xBC, 0xA8};
static char disp_on404[2] = {0xBD, 0x00};
static char disp_on405[2] = {0xBE, 0xB7};
static char disp_on406[2] = {0xBF, 0x00};
static char disp_on407[2] = {0xC0, 0xC5};
static char disp_on408[2] = {0xC1, 0x00};
static char disp_on409[2] = {0xC2, 0xF7};
static char disp_on410[2] = {0xC3, 0x01};
static char disp_on411[2] = {0xC4, 0x1E};
static char disp_on412[2] = {0xC5, 0x01};
static char disp_on413[2] = {0xC6, 0x60};
static char disp_on414[2] = {0xC7, 0x01};
static char disp_on415[2] = {0xC8, 0x95};
static char disp_on416[2] = {0xC9, 0x01};
static char disp_on417[2] = {0xCA, 0xE1};
static char disp_on418[2] = {0xCB, 0x02};
static char disp_on419[2] = {0xCC, 0x20};
static char disp_on420[2] = {0xCD, 0x02};
static char disp_on421[2] = {0xCE, 0x23};
static char disp_on422[2] = {0xCF, 0x02};
static char disp_on423[2] = {0xD0, 0x59};
static char disp_on424[2] = {0xD1, 0x02};
static char disp_on425[2] = {0xD2, 0x94};
static char disp_on426[2] = {0xD3, 0x02};
static char disp_on427[2] = {0xD4, 0xB4};
static char disp_on428[2] = {0xD5, 0x02};
static char disp_on429[2] = {0xD6, 0xE1};
static char disp_on430[2] = {0xD7, 0x03};
static char disp_on431[2] = {0xD8, 0x01};
static char disp_on432[2] = {0xD9, 0x03};
static char disp_on433[2] = {0xDA, 0x28};
static char disp_on434[2] = {0xDB, 0x03};
static char disp_on435[2] = {0xDC, 0x30};
static char disp_on436[2] = {0xDD, 0x03};
static char disp_on437[2] = {0xDE, 0x37};
static char disp_on438[2] = {0xDF, 0x03};
static char disp_on439[2] = {0xE0, 0x3B};
static char disp_on440[2] = {0xE1, 0x03};
static char disp_on441[2] = {0xE2, 0x40};
static char disp_on442[2] = {0xE3, 0x03};
static char disp_on443[2] = {0xE4, 0x50};
static char disp_on444[2] = {0xE5, 0x03};
static char disp_on445[2] = {0xE6, 0x6D};
static char disp_on446[2] = {0xE7, 0x03};
static char disp_on447[2] = {0xE8, 0x80};
static char disp_on448[2] = {0xE9, 0x03};
static char disp_on449[2] = {0xEA, 0xCB};
/* Gamma- B settings end */
static char disp_on450[2] = {0xFF, 0x01};
static char disp_on451[2] = {0xFB, 0x01};
static char disp_on452[2] = {0xFF, 0x02};
static char disp_on453[2] = {0xFB, 0x01};
static char disp_on454[2] = {0xFF, 0x04};
static char disp_on455[2] = {0xFB, 0x01};
static char disp_on456[2] = {0xFF, 0x00};
static char disp_on457[2] = {0x11, 0x00}; /* Sleep out, WaitTime(100) */
static char disp_on458[2] = {0xFF, 0xEE};
static char disp_on459[2] = {0x12, 0x50};
static char disp_on460[2] = {0x13, 0x02};
static char disp_on461[2] = {0x6A, 0x60};
static char disp_on462[2] = {0xFF, 0x00};
static char disp_on463[2] = {0x29, 0x00}; /* Display on */

static struct dsi_cmd_desc nt35590_cmd_display_on_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on0), disp_on0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on1), disp_on1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on2), disp_on2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 10, sizeof(disp_on3), disp_on3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(lane), lane},
	/*{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on_rotate),
		 disp_on_rotate},*/
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(command_mode), command_mode},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on6), disp_on6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on7), disp_on7},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on8), disp_on8},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on9), disp_on9},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on10), disp_on10},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on11), disp_on11},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on12), disp_on12},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on13), disp_on13},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on14), disp_on14},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on15), disp_on15},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on16), disp_on16},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on17), disp_on17},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on18), disp_on18},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on19), disp_on19},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on20), disp_on20},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on21), disp_on21},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on22), disp_on22},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on23), disp_on23},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on24), disp_on24},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on25), disp_on25},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on26), disp_on26},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on27), disp_on27},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on28), disp_on28},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on29), disp_on29},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on30), disp_on30},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on31), disp_on31},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on32), disp_on32},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on33), disp_on33},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on34), disp_on34},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on35), disp_on35},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on36), disp_on36},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on37), disp_on37},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on38), disp_on38},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on39), disp_on39},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on40), disp_on40},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on41), disp_on41},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on42), disp_on42},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on43), disp_on43},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on44), disp_on44},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on45), disp_on45},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on46), disp_on46},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on47), disp_on47},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on48), disp_on48},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on49), disp_on49},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on50), disp_on50},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on51), disp_on51},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on52), disp_on52},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on53), disp_on53},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on54), disp_on54},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on55), disp_on55},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on56), disp_on56},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on57), disp_on57},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on58), disp_on58},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on59), disp_on59},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on60), disp_on60},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on61), disp_on61},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on62), disp_on62},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on63), disp_on63},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on64), disp_on64},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on65), disp_on65},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on66), disp_on66},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on67), disp_on67},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on68), disp_on68},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on69), disp_on69},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on70), disp_on70},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on71), disp_on71},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on72), disp_on72},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on73), disp_on73},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on74), disp_on74},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on75), disp_on75},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on76), disp_on76},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on77), disp_on77},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on78), disp_on78},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on79), disp_on79},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on80), disp_on80},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on81), disp_on81},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on82), disp_on82},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on83), disp_on83},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on84), disp_on84},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on85), disp_on85},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on86), disp_on86},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on87), disp_on87},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on88), disp_on88},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on89), disp_on89},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on90), disp_on90},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on91), disp_on91},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on92), disp_on92},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on93), disp_on93},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on94), disp_on94},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on95), disp_on95},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on96), disp_on96},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on97), disp_on97},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on98), disp_on98},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on99), disp_on99},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on100), disp_on100},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on101), disp_on101},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on102), disp_on102},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on103), disp_on103},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on104), disp_on104},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on105), disp_on105},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on106), disp_on106},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on107), disp_on107},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on108), disp_on108},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on109), disp_on109},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on110), disp_on110},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on111), disp_on111},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on112), disp_on112},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on113), disp_on113},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on114), disp_on114},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on115), disp_on115},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on116), disp_on116},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on117), disp_on117},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on118), disp_on118},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on119), disp_on119},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on120), disp_on120},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on121), disp_on121},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on122), disp_on122},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on123), disp_on123},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on124), disp_on124},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on125), disp_on125},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on126), disp_on126},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on127), disp_on127},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on128), disp_on128},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on129), disp_on129},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on130), disp_on130},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on131), disp_on131},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on132), disp_on132},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on133), disp_on133},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on134), disp_on134},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on135), disp_on135},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on136), disp_on136},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on137), disp_on137},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on138), disp_on138},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on139), disp_on139},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on140), disp_on140},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on141), disp_on141},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on142), disp_on142},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on143), disp_on143},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on144), disp_on144},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on145), disp_on145},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on146), disp_on146},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on147), disp_on147},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on148), disp_on148},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on149), disp_on149},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on150), disp_on150},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on151), disp_on151},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on152), disp_on152},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on153), disp_on153},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on154), disp_on154},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on155), disp_on155},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on156), disp_on156},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on157), disp_on157},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on158), disp_on158},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on159), disp_on159},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on160), disp_on150},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on161), disp_on161},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on162), disp_on162},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on163), disp_on163},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on164), disp_on164},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on165), disp_on165},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on166), disp_on166},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on167), disp_on167},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on168), disp_on168},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on169), disp_on169},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on170), disp_on170},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on171), disp_on171},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on172), disp_on172},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on173), disp_on173},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on174), disp_on174},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on175), disp_on175},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on176), disp_on176},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on177), disp_on177},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on178), disp_on178},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on179), disp_on179},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on180), disp_on180},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on181), disp_on181},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on182), disp_on182},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on183), disp_on183},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on184), disp_on184},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on185), disp_on185},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on186), disp_on186},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on187), disp_on187},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on188), disp_on188},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on189), disp_on189},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on190), disp_on190},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on191), disp_on191},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on192), disp_on192},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on193), disp_on193},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on194), disp_on194},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on195), disp_on195},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on196), disp_on196},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on197), disp_on197},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on198), disp_on198},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on199), disp_on199},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on200), disp_on200},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on201), disp_on201},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on202), disp_on202},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on203), disp_on203},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on204), disp_on204},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on205), disp_on205},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on206), disp_on206},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on207), disp_on207},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on208), disp_on208},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on209), disp_on209},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on210), disp_on210},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on211), disp_on211},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on212), disp_on212},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on213), disp_on213},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on214), disp_on214},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on215), disp_on215},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on216), disp_on216},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on217), disp_on217},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on218), disp_on218},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on219), disp_on219},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on220), disp_on220},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on221), disp_on221},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on222), disp_on222},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on223), disp_on223},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on224), disp_on224},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on225), disp_on225},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on226), disp_on226},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on227), disp_on227},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on228), disp_on228},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on229), disp_on229},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on230), disp_on230},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on231), disp_on231},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on232), disp_on232},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on233), disp_on233},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on234), disp_on234},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on235), disp_on235},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on236), disp_on236},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on237), disp_on237},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on238), disp_on238},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on239), disp_on239},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on240), disp_on240},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on241), disp_on241},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on242), disp_on242},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on243), disp_on243},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on244), disp_on244},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on245), disp_on245},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on246), disp_on246},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on247), disp_on247},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on248), disp_on248},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on249), disp_on249},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on250), disp_on250},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on251), disp_on251},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on252), disp_on252},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on253), disp_on253},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on254), disp_on254},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on255), disp_on255},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on256), disp_on256},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on257), disp_on257},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on258), disp_on258},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on259), disp_on259},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on260), disp_on260},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on261), disp_on261},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on262), disp_on262},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on263), disp_on263},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on264), disp_on264},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on265), disp_on265},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on266), disp_on266},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on267), disp_on267},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on268), disp_on268},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on269), disp_on269},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on270), disp_on270},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on271), disp_on271},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on272), disp_on272},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on273), disp_on273},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on274), disp_on274},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on275), disp_on275},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on276), disp_on276},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on277), disp_on277},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on278), disp_on278},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on279), disp_on279},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on280), disp_on280},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on281), disp_on281},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on282), disp_on282},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on283), disp_on283},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on284), disp_on284},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on285), disp_on285},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on286), disp_on286},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on287), disp_on287},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on288), disp_on288},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on289), disp_on289},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on290), disp_on290},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on291), disp_on291},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on292), disp_on292},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on293), disp_on293},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on294), disp_on294},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on295), disp_on295},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on296), disp_on296},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on297), disp_on297},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on298), disp_on298},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on299), disp_on299},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on300), disp_on300},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on301), disp_on301},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on302), disp_on302},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on303), disp_on303},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on304), disp_on304},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on305), disp_on305},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on306), disp_on306},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on307), disp_on307},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on308), disp_on308},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on309), disp_on309},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on310), disp_on310},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on311), disp_on311},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on312), disp_on312},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on313), disp_on313},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on314), disp_on314},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on315), disp_on315},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on316), disp_on316},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on317), disp_on317},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on318), disp_on318},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on319), disp_on319},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on320), disp_on320},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on321), disp_on321},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on322), disp_on322},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on323), disp_on323},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on324), disp_on324},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on325), disp_on325},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on326), disp_on326},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on327), disp_on327},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on328), disp_on328},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on329), disp_on329},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on330), disp_on330},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on331), disp_on331},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on332), disp_on332},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on333), disp_on333},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on334), disp_on334},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on335), disp_on335},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on336), disp_on336},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on337), disp_on337},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on338), disp_on338},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on339), disp_on339},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on340), disp_on340},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on341), disp_on341},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on342), disp_on342},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on343), disp_on343},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on344), disp_on344},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on345), disp_on345},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on346), disp_on346},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on347), disp_on347},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on348), disp_on348},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on349), disp_on349},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on350), disp_on350},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on351), disp_on351},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on352), disp_on352},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on353), disp_on353},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on354), disp_on354},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on355), disp_on355},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on356), disp_on356},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on357), disp_on357},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on358), disp_on358},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on359), disp_on359},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on360), disp_on360},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on361), disp_on361},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on362), disp_on362},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on363), disp_on363},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on364), disp_on364},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on365), disp_on365},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on366), disp_on366},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on367), disp_on367},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on368), disp_on368},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on369), disp_on369},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on370), disp_on370},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on371), disp_on371},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on372), disp_on372},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on373), disp_on373},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on374), disp_on374},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on375), disp_on375},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on376), disp_on376},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on377), disp_on377},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on378), disp_on378},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on379), disp_on379},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on380), disp_on380},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on381), disp_on381},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on382), disp_on382},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on383), disp_on383},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on384), disp_on384},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on385), disp_on385},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on386), disp_on386},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on387), disp_on387},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on388), disp_on388},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on389), disp_on389},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on390), disp_on390},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on391), disp_on391},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on392), disp_on392},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on393), disp_on393},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on394), disp_on394},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on395), disp_on395},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on396), disp_on396},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on397), disp_on397},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on398), disp_on398},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on399), disp_on399},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on400), disp_on400},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on401), disp_on401},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on402), disp_on402},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on403), disp_on403},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on404), disp_on404},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on405), disp_on405},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on406), disp_on406},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on407), disp_on407},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on408), disp_on408},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on409), disp_on409},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on410), disp_on410},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on411), disp_on411},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on412), disp_on412},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on413), disp_on413},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on414), disp_on414},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on415), disp_on415},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on416), disp_on416},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on417), disp_on417},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on418), disp_on418},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on419), disp_on419},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on420), disp_on420},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on421), disp_on421},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on422), disp_on422},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on423), disp_on423},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on424), disp_on424},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on425), disp_on425},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on426), disp_on426},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on427), disp_on427},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on428), disp_on428},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on429), disp_on429},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on430), disp_on430},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on431), disp_on431},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on432), disp_on432},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on433), disp_on433},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on434), disp_on434},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on435), disp_on435},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on436), disp_on436},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on437), disp_on437},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on438), disp_on438},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on439), disp_on439},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on440), disp_on440},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on441), disp_on441},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on442), disp_on442},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on443), disp_on443},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on444), disp_on444},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on445), disp_on445},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on446), disp_on446},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on447), disp_on447},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on448), disp_on448},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on449), disp_on449},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on450), disp_on450},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on451), disp_on451},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on452), disp_on452},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on453), disp_on453},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on454), disp_on454},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on455), disp_on455},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on456), disp_on456},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 100, sizeof(disp_on457), disp_on457},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on458), disp_on458},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on459), disp_on459},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on460), disp_on460},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on461), disp_on461},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on462), disp_on462},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 120, sizeof(disp_on463), disp_on463},
};

static struct dsi_cmd_desc nt35590_video_display_on_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on0), disp_on0},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on1), disp_on1},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on2), disp_on2},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 10, sizeof(disp_on3), disp_on3},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(lane), lane},
	/*{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on_rotate),
		disp_on_rotate},*/
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(video_mode), video_mode},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on6), disp_on6},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on7), disp_on7},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on8), disp_on8},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on9), disp_on9},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on10), disp_on10},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on11), disp_on11},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on12), disp_on12},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on13), disp_on13},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on14), disp_on14},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on15), disp_on15},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on16), disp_on16},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on17), disp_on17},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on18), disp_on18},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on19), disp_on19},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on20), disp_on20},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on21), disp_on21},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on22), disp_on22},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on23), disp_on23},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on24), disp_on24},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on25), disp_on25},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on26), disp_on26},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on27), disp_on27},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on28), disp_on28},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on29), disp_on29},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on30), disp_on30},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on31), disp_on31},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on32), disp_on32},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on33), disp_on33},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on34), disp_on34},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on35), disp_on35},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on36), disp_on36},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on37), disp_on37},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on38), disp_on38},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on39), disp_on39},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on40), disp_on40},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on41), disp_on41},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on42), disp_on42},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on43), disp_on43},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on44), disp_on44},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on45), disp_on45},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on46), disp_on46},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on47), disp_on47},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on48), disp_on48},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on49), disp_on49},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on50), disp_on50},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on51), disp_on51},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on52), disp_on52},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on53), disp_on53},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on54), disp_on54},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on55), disp_on55},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on56), disp_on56},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on57), disp_on57},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on58), disp_on58},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on59), disp_on59},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on60), disp_on60},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on61), disp_on61},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on62), disp_on62},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on63), disp_on63},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on64), disp_on64},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on65), disp_on65},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on66), disp_on66},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on67), disp_on67},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on68), disp_on68},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on69), disp_on69},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on70), disp_on70},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on71), disp_on71},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on72), disp_on72},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on73), disp_on73},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on74), disp_on74},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on75), disp_on75},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on76), disp_on76},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on77), disp_on77},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on78), disp_on78},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on79), disp_on79},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on80), disp_on80},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on81), disp_on81},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on82), disp_on82},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on83), disp_on83},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on84), disp_on84},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on85), disp_on85},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on86), disp_on86},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on87), disp_on87},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on88), disp_on88},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on89), disp_on89},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on90), disp_on90},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on91), disp_on91},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on92), disp_on92},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on93), disp_on93},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on94), disp_on94},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on95), disp_on95},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on96), disp_on96},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on97), disp_on97},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on98), disp_on98},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on99), disp_on99},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on100), disp_on100},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on101), disp_on101},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on102), disp_on102},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on103), disp_on103},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on104), disp_on104},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on105), disp_on105},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on106), disp_on106},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on107), disp_on107},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on108), disp_on108},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on109), disp_on109},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on110), disp_on110},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on111), disp_on111},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on112), disp_on112},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on113), disp_on113},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on114), disp_on114},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on115), disp_on115},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on116), disp_on116},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on117), disp_on117},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on118), disp_on118},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on119), disp_on119},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on120), disp_on120},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on121), disp_on121},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on122), disp_on122},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on123), disp_on123},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on124), disp_on124},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on125), disp_on125},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on126), disp_on126},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on127), disp_on127},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on128), disp_on128},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on129), disp_on129},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on130), disp_on130},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on131), disp_on131},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on132), disp_on132},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on133), disp_on133},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on134), disp_on134},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on135), disp_on135},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on136), disp_on136},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on137), disp_on137},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on138), disp_on138},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on139), disp_on139},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on140), disp_on140},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on141), disp_on141},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on142), disp_on142},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on143), disp_on143},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on144), disp_on144},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on145), disp_on145},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on146), disp_on146},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on147), disp_on147},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on148), disp_on148},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on149), disp_on149},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on150), disp_on150},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on151), disp_on151},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on152), disp_on152},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on153), disp_on153},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on154), disp_on154},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on155), disp_on155},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on156), disp_on156},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on157), disp_on157},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on158), disp_on158},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on159), disp_on159},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on160), disp_on150},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on161), disp_on161},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on162), disp_on162},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on163), disp_on163},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on164), disp_on164},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on165), disp_on165},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on166), disp_on166},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on167), disp_on167},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on168), disp_on168},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on169), disp_on169},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on170), disp_on170},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on171), disp_on171},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on172), disp_on172},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on173), disp_on173},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on174), disp_on174},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on175), disp_on175},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on176), disp_on176},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on177), disp_on177},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on178), disp_on178},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on179), disp_on179},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on180), disp_on180},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on181), disp_on181},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on182), disp_on182},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on183), disp_on183},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on184), disp_on184},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on185), disp_on185},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on186), disp_on186},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on187), disp_on187},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on188), disp_on188},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on189), disp_on189},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on190), disp_on190},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on191), disp_on191},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on192), disp_on192},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on193), disp_on193},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on194), disp_on194},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on195), disp_on195},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on196), disp_on196},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on197), disp_on197},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on198), disp_on198},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on199), disp_on199},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on200), disp_on200},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on201), disp_on201},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on202), disp_on202},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on203), disp_on203},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on204), disp_on204},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on205), disp_on205},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on206), disp_on206},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on207), disp_on207},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on208), disp_on208},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on209), disp_on209},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on210), disp_on210},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on211), disp_on211},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on212), disp_on212},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on213), disp_on213},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on214), disp_on214},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on215), disp_on215},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on216), disp_on216},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on217), disp_on217},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on218), disp_on218},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on219), disp_on219},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on220), disp_on220},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on221), disp_on221},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on222), disp_on222},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on223), disp_on223},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on224), disp_on224},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on225), disp_on225},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on226), disp_on226},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on227), disp_on227},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on228), disp_on228},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on229), disp_on229},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on230), disp_on230},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on231), disp_on231},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on232), disp_on232},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on233), disp_on233},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on234), disp_on234},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on235), disp_on235},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on236), disp_on236},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on237), disp_on237},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on238), disp_on238},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on239), disp_on239},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on240), disp_on240},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on241), disp_on241},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on242), disp_on242},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on243), disp_on243},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on244), disp_on244},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on245), disp_on245},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on246), disp_on246},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on247), disp_on247},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on248), disp_on248},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on249), disp_on249},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on250), disp_on250},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on251), disp_on251},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on252), disp_on252},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on253), disp_on253},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on254), disp_on254},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on255), disp_on255},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on256), disp_on256},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on257), disp_on257},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on258), disp_on258},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on259), disp_on259},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on260), disp_on260},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on261), disp_on261},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on262), disp_on262},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on263), disp_on263},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on264), disp_on264},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on265), disp_on265},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on266), disp_on266},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on267), disp_on267},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on268), disp_on268},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on269), disp_on269},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on270), disp_on270},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on271), disp_on271},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on272), disp_on272},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on273), disp_on273},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on274), disp_on274},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on275), disp_on275},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on276), disp_on276},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on277), disp_on277},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on278), disp_on278},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on279), disp_on279},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on280), disp_on280},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on281), disp_on281},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on282), disp_on282},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on283), disp_on283},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on284), disp_on284},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on285), disp_on285},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on286), disp_on286},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on287), disp_on287},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on288), disp_on288},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on289), disp_on289},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on290), disp_on290},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on291), disp_on291},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on292), disp_on292},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on293), disp_on293},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on294), disp_on294},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on295), disp_on295},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on296), disp_on296},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on297), disp_on297},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on298), disp_on298},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on299), disp_on299},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on300), disp_on300},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on301), disp_on301},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on302), disp_on302},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on303), disp_on303},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on304), disp_on304},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on305), disp_on305},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on306), disp_on306},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on307), disp_on307},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on308), disp_on308},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on309), disp_on309},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on310), disp_on310},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on311), disp_on311},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on312), disp_on312},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on313), disp_on313},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on314), disp_on314},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on315), disp_on315},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on316), disp_on316},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on317), disp_on317},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on318), disp_on318},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on319), disp_on319},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on320), disp_on320},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on321), disp_on321},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on322), disp_on322},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on323), disp_on323},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on324), disp_on324},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on325), disp_on325},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on326), disp_on326},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on327), disp_on327},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on328), disp_on328},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on329), disp_on329},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on330), disp_on330},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on331), disp_on331},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on332), disp_on332},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on333), disp_on333},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on334), disp_on334},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on335), disp_on335},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on336), disp_on336},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on337), disp_on337},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on338), disp_on338},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on339), disp_on339},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on340), disp_on340},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on341), disp_on341},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on342), disp_on342},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on343), disp_on343},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on344), disp_on344},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on345), disp_on345},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on346), disp_on346},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on347), disp_on347},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on348), disp_on348},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on349), disp_on349},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on350), disp_on350},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on351), disp_on351},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on352), disp_on352},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on353), disp_on353},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on354), disp_on354},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on355), disp_on355},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on356), disp_on356},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on357), disp_on357},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on358), disp_on358},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on359), disp_on359},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on360), disp_on360},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on361), disp_on361},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on362), disp_on362},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on363), disp_on363},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on364), disp_on364},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on365), disp_on365},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on366), disp_on366},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on367), disp_on367},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on368), disp_on368},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on369), disp_on369},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on370), disp_on370},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on371), disp_on371},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on372), disp_on372},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on373), disp_on373},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on374), disp_on374},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on375), disp_on375},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on376), disp_on376},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on377), disp_on377},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on378), disp_on378},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on379), disp_on379},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on380), disp_on380},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on381), disp_on381},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on382), disp_on382},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on383), disp_on383},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on384), disp_on384},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on385), disp_on385},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on386), disp_on386},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on387), disp_on387},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on388), disp_on388},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on389), disp_on389},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on390), disp_on390},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on391), disp_on391},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on392), disp_on392},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on393), disp_on393},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on394), disp_on394},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on395), disp_on395},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on396), disp_on396},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on397), disp_on397},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on398), disp_on398},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on399), disp_on399},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on400), disp_on400},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on401), disp_on401},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on402), disp_on402},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on403), disp_on403},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on404), disp_on404},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on405), disp_on405},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on406), disp_on406},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on407), disp_on407},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on408), disp_on408},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on409), disp_on409},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on410), disp_on410},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on411), disp_on411},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on412), disp_on412},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on413), disp_on413},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on414), disp_on414},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on415), disp_on415},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on416), disp_on416},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on417), disp_on417},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on418), disp_on418},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on419), disp_on419},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on420), disp_on420},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on421), disp_on421},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on422), disp_on422},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on423), disp_on423},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on424), disp_on424},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on425), disp_on425},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on426), disp_on426},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on427), disp_on427},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on428), disp_on428},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on429), disp_on429},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on430), disp_on430},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on431), disp_on431},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on432), disp_on432},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on433), disp_on433},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on434), disp_on434},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on435), disp_on435},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on436), disp_on436},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on437), disp_on437},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on438), disp_on438},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on439), disp_on439},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on440), disp_on440},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on441), disp_on441},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on442), disp_on442},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on443), disp_on443},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on444), disp_on444},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on445), disp_on445},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on446), disp_on446},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on447), disp_on447},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on448), disp_on448},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on449), disp_on449},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on450), disp_on450},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on451), disp_on451},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on452), disp_on452},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on453), disp_on453},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on454), disp_on454},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on455), disp_on455},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on456), disp_on456},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 100, sizeof(disp_on457), disp_on457},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on458), disp_on458},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on459), disp_on459},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on460), disp_on460},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on461), disp_on461},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disp_on462), disp_on462},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 120, sizeof(disp_on463), disp_on463},
};

static int mipi_nt35590_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	pr_err("%s: Enter\n", __func__);
	mipi  = &mfd->panel_info.mipi;

	if (mipi->mode == DSI_VIDEO_MODE) {
		mipi_dsi_cmds_tx(&nt35590_tx_buf,
			nt35590_video_display_on_cmds,
			ARRAY_SIZE(nt35590_video_display_on_cmds));
	} else if (mipi->mode == DSI_CMD_MODE) {
		mipi_dsi_cmds_tx(&nt35590_tx_buf,
			nt35590_cmd_display_on_cmds,
			ARRAY_SIZE(nt35590_cmd_display_on_cmds));
	}
	pr_err("%s: Exit\n", __func__);

	return 0;
}

static int mipi_nt35590_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	pr_err("mipi_nt35590_lcd_off E\n");

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi_dsi_cmds_tx(&nt35590_tx_buf, nt35590_display_off_cmds,
			ARRAY_SIZE(nt35590_display_off_cmds));

	pr_err("mipi_nt35590_lcd_off X\n");
	return 0;
}

static int __devinit mipi_nt35590_lcd_probe(struct platform_device *pdev)
{
	struct platform_device *pthisdev = NULL;
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;

	if (pdev->id == 0) {
		/* No board specific panel data as of now */
		return 0;
	}

	pthisdev = msm_fb_add_device(pdev);
	if (pthisdev) {
		mfd = platform_get_drvdata(pthisdev);
		if (!mfd)
			return -ENODEV;
		if (mfd->key != MFD_KEY)
			return -EINVAL;

		mipi  = &mfd->panel_info.mipi;

		mipi->dlane_swap = 0;
	}

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_nt35590_lcd_probe,
	.driver = {
		.name   = "mipi_NT35590",
	},
};

DEFINE_LED_TRIGGER(bkl_led_trigger);
static void mipi_nt35590_set_backlight(struct msm_fb_data_type *mfd)
{
	if (wled_trigger_initialized)
		led_trigger_event(bkl_led_trigger, mfd->bl_level);
	return;
}

static struct msm_fb_panel_data nt35590_panel_data = {
	.on	= mipi_nt35590_lcd_on,
	.off = mipi_nt35590_lcd_off,
	.set_backlight = mipi_nt35590_set_backlight,
};

static int ch_used[3];

static int mipi_nt35590_lcd_init(void)
{
	mipi_dsi_buf_alloc(&nt35590_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&nt35590_rx_buf, DSI_BUF_SIZE);

	led_trigger_register_simple("bkl_trigger", &bkl_led_trigger);
	pr_info("%s: SUCCESS (WLED TRIGGER)\n", __func__);
	wled_trigger_initialized = 1;

	return platform_driver_register(&this_driver);
}

int mipi_nt35590_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	ret = mipi_nt35590_lcd_init();
	if (ret) {
		pr_err("mipi_nt35590_lcd_init() failed with ret %u\n", ret);
		return ret;
	}

	pdev = platform_device_alloc("mipi_NT35590", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	nt35590_panel_data.panel_info = *pinfo;
	ret = platform_device_add_data(pdev, &nt35590_panel_data,
				sizeof(nt35590_panel_data));
	if (ret) {
		pr_debug("%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_debug("%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}
