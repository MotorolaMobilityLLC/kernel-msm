#ifndef __ALS_H
#define __ALS_H

#define NUM_ALS_ZONES 6 // 5 zones and 1 undefined

typedef void (*als_cb)(unsigned prev_zone, unsigned curr_zone, uint32_t cookie);
int lm3535_register_als_callback(als_cb func, uint32_t cookie);
void lm3535_unregister_als_callback(als_cb func);
int adp8862_register_als_callback(als_cb func, uint32_t cookie);
void adp8862_unregister_als_callback(als_cb func);
unsigned lm3535_als_is_dark(void);
unsigned adp8862_als_is_dark(void);
typedef unsigned (*als_is_dark_func)(void);
#endif
