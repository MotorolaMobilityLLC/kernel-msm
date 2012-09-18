/*
 * File: tspdrv.h
 *
 * Description:
 *     Constants and type definitions for the TouchSense Kernel Module.
 *
 * Portions Copyright (c) 2008-2010 Immersion Corporation. All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the GNU Public License v2 -
 * (the 'License'). You may not use this file except in compliance with the
 * License. You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
 * TouchSenseSales@immersion.com.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
 * the License for the specific language governing rights and limitations
 * under the License.
 */

#ifndef _TSPDRV_H
#define _TSPDRV_H

/* Constants */
#define MODULE_NAME                         "tspdrv"
#define TSPDRV                              "/dev/"MODULE_NAME
#define TSPDRV_MAGIC_NUMBER                 0x494D4D52
#define TSPDRV_STOP_KERNEL_TIMER            _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 1)
/*
** Obsolete IOCTL command
** #define TSPDRV_IDENTIFY_CALLER           _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 2)
*/
#define TSPDRV_ENABLE_AMP                   _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 3)
#define TSPDRV_DISABLE_AMP                  _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 4)
#define TSPDRV_GET_NUM_ACTUATORS            _IO(TSPDRV_MAGIC_NUMBER & 0xFF, 5)
#define VIBE_MAX_DEVICE_NAME_LENGTH         64
#define SPI_HEADER_SIZE                     3   /* DO NOT CHANGE - SPI buffer header size */
#define VIBE_OUTPUT_SAMPLE_SIZE             50  /* DO NOT CHANGE - maximum number of samples */

/* Type definitions */
#ifdef __KERNEL__
typedef int8_t		VibeInt8;
typedef u_int8_t	VibeUInt8;
typedef int16_t		VibeInt16;
typedef u_int16_t	VibeUInt16;
typedef int32_t		VibeInt32;
typedef u_int32_t	VibeUInt32;
typedef u_int8_t	VibeBool;
typedef VibeInt32	VibeStatus;

typedef struct {
	VibeUInt8 nActuatorIndex;  /* 1st byte is actuator index */
	VibeUInt8 nBitDepth;       /* 2nd byte is bit depth */
	VibeUInt8 nBufferSize;     /* 3rd byte is data size */
	VibeUInt8 dataBuffer[VIBE_OUTPUT_SAMPLE_SIZE];
} samples_buffer;

typedef struct {
	VibeInt8 nIndexPlayingBuffer;
	VibeUInt8 nIndexOutputValue;
	samples_buffer actuatorSamples[2]; /* Use 2 buffers to receive samples from user mode */
} actuator_samples_buffer;

#endif

/* Error and Return value codes */
#define VIBE_S_SUCCESS                      0   /* Success */
#define VIBE_E_FAIL                         -4  /* Generic error */

#if defined(VIBE_RECORD) && defined(VIBE_DEBUG)
void _RecorderInit(void);
void _RecorderTerminate(void);
void _RecorderReset(int nActuator);
void _Record(int actuatorIndex, const char *format,...);
#endif

/* Kernel Debug Macros */
#ifdef __KERNEL__
    #if 1 // def VIBE_DEBUG
        #define DbgOut(_x_) printk _x_
    #else   /* VIBE_DEBUG */
        #define DbgOut(_x_)
    #endif  /* VIBE_DEBUG */

    #if defined(VIBE_RECORD) && defined(VIBE_DEBUG)
        #define DbgRecorderInit(_x_) _RecorderInit _x_
        #define DbgRecorderTerminate(_x_) _RecorderTerminate _x_
        #define DbgRecorderReset(_x_) _RecorderReset _x_
        #define DbgRecord(_x_) _Record _x_
    #else /* defined(VIBE_RECORD) && defined(VIBE_DEBUG) */
        #define DbgRecorderInit(_x_)
        #define DbgRecorderTerminate(_x_)
        #define DbgRecorderReset(_x_)
        #define DbgRecord(_x_)
    #endif /* defined(VIBE_RECORD) && defined(VIBE_DEBUG) */
#endif  /* __KERNEL__ */

#endif  /* _TSPDRV_H */
