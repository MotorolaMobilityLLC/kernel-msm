/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */  
    
#ifndef LINUX_SPI_FPC1020_DEBUG_H
#define LINUX_SPI_FPC1020_DEBUG_H
    
#include <linux/types.h>
    
/** Store grayscale image for debug */ 
void fpc1020_debug_store_image(u8 * image, int width, int height);
 
#endif	/* LINUX_SPI_FPC1020_DEBUG_H */
