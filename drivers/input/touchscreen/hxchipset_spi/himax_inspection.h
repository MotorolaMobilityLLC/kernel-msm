
#include "himax_platform.h"
#include "himax_common.h"
#include "himax_ic_core.h"

/*#define HX_GAP_TEST*/
/*#define HX_INSPECT_LPWUG_TEST*/
/*#define HX_ACT_IDLE_TEST*/

#define HX_RSLT_OUT_PATH "/sdcard/"
#define HX_RSLT_OUT_FILE "hx_test_result.txt"
char *g_file_path;
char *g_rslt_data;


#define BS_RAWDATANOISE    10
#define BS_OPENSHORT       0


#define    BS_LPWUG        1
#define    BS_LPWUG_dile   1



#define    BS_ACT_IDLE     1


/* skip notch & dummy */
#define SKIP_NOTCH_START  5
#define SKIP_NOTCH_END    10
/* TX+SKIP_NOTCH_START */
#define SKIP_DUMMY_START  23
/* TX+SKIP_NOTCH_END*/
#define SKIP_DUMMY_END    28


#define    NOISEFRAME    (BS_RAWDATANOISE+1)
#define    UNIFMAX       500


/*Himax MP Password*/
#define    PWD_OPEN_START    0x77
#define    PWD_OPEN_END      0x88
#define    PWD_SHORT_START   0x11
#define    PWD_SHORT_END     0x33
#define    PWD_RAWDATA_START 0x00
#define    PWD_RAWDATA_END   0x99
#define    PWD_NOISE_START   0x00
#define    PWD_NOISE_END     0x99
#define    PWD_SORTING_START 0xAA
#define    PWD_SORTING_END   0xCC


#define PWD_ACT_IDLE_START   0x22
#define PWD_ACT_IDLE_END     0x44



#define PWD_LPWUG_START      0x55
#define PWD_LPWUG_END        0x66

#define PWD_LPWUG_IDLE_START 0x50
#define PWD_LPWUG_IDLE_END   0x60


/*Himax DataType*/
#define DATA_SORTING         0x0A
#define DATA_OPEN            0x0B
#define DATA_MICRO_OPEN      0x0C
#define DATA_SHORT           0x0A
#define DATA_RAWDATA         0x0A
#define DATA_NOISE           0x0F
#define DATA_BACK_NORMAL     0x00
#define DATA_LPWUG_RAWDATA   0x0C
#define DATA_LPWUG_NOISE     0x0F
#define DATA_ACT_IDLE_RAWDATA 0x0A
#define DATA_ACT_IDLE_NOISE  0x0F
#define DATA_LPWUG_IDLE_RAWDATA 0x0A
#define DATA_LPWUG_IDLE_NOISE   0x0F

/*Himax Data Ready Password*/
#define    Data_PWD0    0xA5
#define    Data_PWD1    0x5A

/* ASCII format */
#define ASCII_LF    (0x0A)
#define ASCII_CR    (0x0D)
#define ASCII_COMMA (0x2C)
#define ASCII_ZERO  (0x30)
#define CHAR_EL    '\0'
#define CHAR_NL    '\n'
#define ACSII_SPACE (0x20)
/* INSOECTION Setting */
int HX_CRITERIA_ITEM;
int HX_CRITERIA_SIZE;

typedef enum {
    HIMAX_INSPECTION_OPEN,
    HIMAX_INSPECTION_MICRO_OPEN,
    HIMAX_INSPECTION_SHORT,
    HIMAX_INSPECTION_RAWDATA,
    HIMAX_INSPECTION_NOISE,
    HIMAX_INSPECTION_SORTING,
    HIMAX_INSPECTION_BACK_NORMAL,

    HIMAX_INSPECTION_GAPTEST_RAW,

    HIMAX_INSPECTION_ACT_IDLE_RAWDATA,
    HIMAX_INSPECTION_ACT_IDLE_NOISE,

    HIMAX_INSPECTION_LPWUG_RAWDATA,
    HIMAX_INSPECTION_LPWUG_NOISE,
    HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA,
    HIMAX_INSPECTION_LPWUG_IDLE_NOISE,

} THP_INSPECTION_ENUM;

char *g_himax_inspection_mode[] = {
    "HIMAX_INSPECTION_OPEN",
    "HIMAX_INSPECTION_MICRO_OPEN",
    "HIMAX_INSPECTION_SHORT",
    "HIMAX_INSPECTION_RAWDATA",
    "HIMAX_INSPECTION_NOISE",
    "HIMAX_INSPECTION_SORTING",
    "HIMAX_INSPECTION_BACK_NORMAL",

    "HIMAX_INSPECTION_GAPTEST_RAW",

    "HIMAX_INSPECTION_ACT_IDLE_RAWDATA",
    "HIMAX_INSPECTION_ACT_IDLE_NOISE",

    "HIMAX_INSPECTION_LPWUG_RAWDATA",
    "HIMAX_INSPECTION_LPWUG_NOISE",
    "HIMAX_INSPECTION_LPWUG_IDLE_RAWDATA",
    "HIMAX_INSPECTION_LPWUG_IDLE_NOISE",
    NULL

};

/* for criteria */
int **g_inspection_criteria;
int *g_inspt_crtra_flag;
char *g_hx_inspt_crtra_name[] = {
    "CRITERIA_RAW_MIN",
    "CRITERIA_RAW_MAX",
    "CRITERIA_SHORT_MIN",
    "CRITERIA_SHORT_MAX",
    "CRITERIA_OPEN_MIN",
    "CRITERIA_OPEN_MAX",
    "CRITERIA_MICRO_OPEN_MIN",
    "CRITERIA_MICRO_OPEN_MAX",
    "CRITERIA_NOISE",
    "CRITERIA_SORT_MIN",
    "CRITERIA_SORT_MAX",

    "CRITERIA_GAP_RAW_MIN",
    "CRITERIA_GAP_RAW_MAX",

    "ACT_IDLE_NOISE_MIN",
    "ACT_IDLE_NOISE_MAX",
    "ACT_IDLE_RAWDATA_MIN",
    "ACT_IDLE_RAWDATA_MAX",

    "LPWUG_NOISE_MIN",
    "LPWUG_NOISE_MAX",
    "LPWUG_RAWDATA_MIN",
    "LPWUG_RAWDATA_MAX",
    "LPWUG_IDLE_NOISE_MIN",
    "LPWUG_IDLE_NOISE_MAX",
    "LPWUG_IDLE_RAWDATA_MIN",
    "LPWUG_IDLE_RAWDATA_MAX",
    NULL

};
typedef enum {
    IDX_RAWMIN        = 0,
    IDX_RAWMAX,
    IDX_SHORTMIN,
    IDX_SHORTMAX,
    IDX_OPENMIN,
    IDX_OPENMAX,
    IDX_M_OPENMIN,
    IDX_M_OPENMAX,
    IDX_NOISEMAX,
    IDX_SORTMIN,
    IDX_SORTMAX,

    IDX_GAP_RAWMAX,
    IDX_GAP_RAWMIN,

    IDX_ACT_IDLE_NOISE_MIN,
    IDX_ACT_IDLE_NOISE_MAX,
    IDX_ACT_IDLE_RAWDATA_MIN,
    IDX_ACT_IDLE_RAWDATA_MAX,

    IDX_LPWUG_NOISE_MIN,
    IDX_LPWUG_NOISE_MAX,
    IDX_LPWUG_RAWDATA_MIN,
    IDX_LPWUG_RAWDATA_MAX,

    IDX_LPWUG_IDLE_NOISE_MIN,
    IDX_LPWUG_IDLE_NOISE_MAX,
    IDX_LPWUG_IDLE_RAWDATA_MIN,
    IDX_LPWUG_IDLE_RAWDATA_MAX,

} HX_CRITERIA_ENUM;

/* Error code of Inspection */
typedef enum {
    HX_INSPECT_OK       = 0,                 /* OK */
    HX_INSPECT_ESPI     = 1 << 1,            /* SPI communication error */
    HX_INSPECT_ERAW     = 2 << 1,            /* Raw data error */
    HX_INSPECT_ENOISE   = 3 << 1,            /* Noise error */
    HX_INSPECT_EOPEN    = 4 << 1,            /* Sensor open error */
    HX_INSPECT_EMOPEN   = 5 << 1,            /* Sensor micro open error */
    HX_INSPECT_ESHORT   = 6 << 1,            /* Sensor short error */
    HX_INSPECT_EGAP_RAW = 8 << 1,            /* Sensor short error */
    HX_INSPECT_EACT_IDLE_RAW   = 9 << 1,     /* ACT_IDLE RAW ERROR */
    HX_INSPECT_EACT_IDLE_NOISE = 10 << 1,    /* ACT_IDLE NOISE ERROR */
    HX_INSPECT_ELPWUG_RAW        = 11 << 1,  /* LPWUG RAW ERROR */
    HX_INSPECT_ELPWUG_NOISE      = 12 << 1,  /* LPWUG NOISE ERROR */
    HX_INSPECT_ELPWUG_IDLE_RAW   = 13 << 1,  /* LPWUG IDLE RAW ERROR */
    HX_INSPECT_ELPWUG_IDLE_NOISE = 14 << 1,  /* LPWUG IDLE NOISE ERROR */
    HX_INSPECT_EFILE      = 15 << 1,         /*Criteria file error*/
    HX_INSPECT_EOTHER     = 16 << 1,         /* All other errors */
} HX_INSPECT_ERR_ENUM;
