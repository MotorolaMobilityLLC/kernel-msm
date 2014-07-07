#ifndef __AXC_IDTP9023Charger_H__
#define __AXC_IDTP9023Charger_H__
typedef enum
{
    IDTP9023_WC_RESET,
    IDTP9023_WC_PD_DET,
    IDTP9023_PIN_COUNT
}IDTP9023_PIN_DEF;

typedef struct IDTP9023_PIN {
    int gpio;
    const char * name;
    int irq;
    int in_out_flag;
    irqreturn_t (*handler)(int, void *);
    int init_value;
    unsigned long trigger_flag;
    bool irq_enabled;
}IDTP9023_PIN;
#endif //__AXC_IDTP9023Charger_H__

