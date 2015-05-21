#ifndef __AXC_BQ51220Charger_H__
#define __AXC_BQ51220Charger_H__
typedef enum
{
    Bq51220_WC_POK,
    Bq51220_PD_DET,
    Bq51220_EN1,
    Bq51220_EN2,
    Bq51220_PIN_COUNT
}Bq51220_PIN_DEF;

typedef struct bq51220_PIN {
    int gpio;
    const char * name;
    int irq;
    int in_out_flag;
    irqreturn_t (*handler)(int, void *);
    int init_value;
    unsigned long trigger_flag;
    bool irq_enabled;
}Bq51220_PIN;
#endif //__AXC_BQ51220Charger_H__

