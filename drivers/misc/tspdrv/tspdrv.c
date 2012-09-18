/*
 * File: tspdrv.c
 *
 * Description:
 *     TouchSense Kernel Module main entry-point.
 *
 * Portions Copyright (c) 2008-2011 Immersion Corporation. All Rights Reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <tspdrv.h>
#include <ImmVibeSPI.c>
#if defined(VIBE_DEBUG) && defined(VIBE_RECORD)
#include <tspdrvRecorder.c>
#endif

/* Device name and version information */
#define VERSION_STR " v3.4.55.8\n" /* DO NOT CHANGE - this is auto-generated */
#define VERSION_STR_LEN 16         /* account extra space for future extra digits in version number */
static char g_szDeviceName[  (VIBE_MAX_DEVICE_NAME_LENGTH
                            + VERSION_STR_LEN)
                            * NUM_ACTUATORS]; /* initialized in init_module */
static size_t g_cchDeviceName;                /* initialized in init_module */

/* Flag indicating whether the driver is in use */
static char g_bIsPlaying = false;

/* Buffer to store data sent to SPI */
#define SPI_BUFFER_SIZE (NUM_ACTUATORS * (VIBE_OUTPUT_SAMPLE_SIZE + SPI_HEADER_SIZE))
static int g_bStopRequested = false;
static actuator_samples_buffer g_SamplesBuffer[NUM_ACTUATORS] = {{0}};
static char g_cWriteBuffer[SPI_BUFFER_SIZE];

/* For QA purposes */
#ifdef QA_TEST
#define FORCE_LOG_BUFFER_SIZE   128
#define TIME_INCREMENT          5
static int g_nTime = 0;
static int g_nForceLogIndex = 0;
static VibeInt8 g_nForceLog[FORCE_LOG_BUFFER_SIZE];
#endif

#if ((LINUX_VERSION_CODE & 0xFFFF00) < KERNEL_VERSION(2,6,0))
#error Unsupported Kernel version
#endif

#ifndef HAVE_UNLOCKED_IOCTL
#define HAVE_UNLOCKED_IOCTL 0
#endif

#ifdef IMPLEMENT_AS_CHAR_DRIVER
static int g_nMajor = 0;
#endif

/* Needs to be included after the global variables because it uses them */
#ifdef CONFIG_HIGH_RES_TIMERS
#include <VibeOSKernelLinuxHRTime.c>
#else
#include <VibeOSKernelLinuxTime.c>
#endif

/* File IO */
static int open(struct inode *inode, struct file *file);
static int release(struct inode *inode, struct file *file);
static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos);
#if HAVE_UNLOCKED_IOCTL
static long unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#else
static int ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
#endif
static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.read = read,
	.write = write,
#if HAVE_UNLOCKED_IOCTL
	.unlocked_ioctl = unlocked_ioctl,
#else
	.ioctl = ioctl,
#endif
	.open = open,
	.release = release,
	.llseek = default_llseek /* using default implementation as declared in linux/fs.h */
};

#ifndef IMPLEMENT_AS_CHAR_DRIVER
static struct miscdevice miscdev =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = MODULE_NAME,
	.fops = &fops
};
#endif

static int suspend(struct platform_device *pdev, pm_message_t state);
static int resume(struct platform_device *pdev);
static struct platform_driver platdrv =
{
	.suspend = suspend,
	.resume = resume,
	.driver = {
		.name = MODULE_NAME,
	},
};

static void platform_release(struct device *dev);
static struct platform_device platdev =
{
	.name =     MODULE_NAME,
	.id =       -1,
	.dev = {
		.platform_data = NULL,
		.release = platform_release,
	},
};


int __init tspdrv_init(void)
{
	int nRet, i;   /* initialized below */

	DbgOut((KERN_INFO "tspdrv: init_module.\n"));

#ifdef IMPLEMENT_AS_CHAR_DRIVER
	g_nMajor = register_chrdev(0, MODULE_NAME, &fops);
	if (g_nMajor < 0) {
		DbgOut((KERN_ERR "tspdrv: can't get major number.\n"));
		return g_nMajor;
	}
#else
	nRet = misc_register(&miscdev);
	if (nRet) {
		DbgOut((KERN_ERR "tspdrv: misc_register failed.\n"));
		return nRet;
	}
#endif

	nRet = platform_device_register(&platdev);
	if (nRet) {
		DbgOut((KERN_ERR "tspdrv: platform_device_register failed.\n"));
	}

	nRet = platform_driver_register(&platdrv);
	if (nRet) {
		DbgOut((KERN_ERR "tspdrv: platform_driver_register failed.\n"));
	}

	DbgRecorderInit(());

	ImmVibeSPI_ForceOut_Initialize();
	VibeOSKernelLinuxInitTimer();

	/* Get and concatenate device name and initialize data buffer */
	g_cchDeviceName = 0;
	for (i = 0; i < NUM_ACTUATORS; i++) {
		char *szName = g_szDeviceName + g_cchDeviceName;
		ImmVibeSPI_Device_GetName(i, szName, VIBE_MAX_DEVICE_NAME_LENGTH);

		/* Append version information and get buffer length */
		strcat(szName, VERSION_STR);
		g_cchDeviceName += strlen(szName);

		g_SamplesBuffer[i].nIndexPlayingBuffer = -1; /* Not playing */
		g_SamplesBuffer[i].actuatorSamples[0].nBufferSize = 0;
		g_SamplesBuffer[i].actuatorSamples[1].nBufferSize = 0;
	}

	return 0;
}

void __exit tspdrv_exit(void)
{
	DbgOut((KERN_INFO "tspdrv: cleanup_module.\n"));

	DbgRecorderTerminate(());

	VibeOSKernelLinuxTerminateTimer();
	ImmVibeSPI_ForceOut_Terminate();

	platform_driver_unregister(&platdrv);
	platform_device_unregister(&platdev);

#ifdef IMPLEMENT_AS_CHAR_DRIVER
	unregister_chrdev(g_nMajor, MODULE_NAME);
#else
	misc_deregister(&miscdev);
#endif
}

static int open(struct inode *inode, struct file *file)
{
	DbgOut((KERN_INFO "tspdrv: open.\n"));

	if (!try_module_get(THIS_MODULE)) return -ENODEV;

	return 0;
}

static int release(struct inode *inode, struct file *file)
{
	DbgOut((KERN_INFO "tspdrv: release.\n"));

	/*
	 * Reset force and stop timer when the driver is closed, to make sure
	 * no dangling semaphore remains in the system, especially when the
	 * driver is run outside of immvibed for testing purposes.
	 */
	VibeOSKernelLinuxStopTimer();

	/*
	 * Clear the variable used to store the magic number to prevent
	 * unauthorized caller to write data. TouchSense service is the only
	 * valid caller.
	 */
	file->private_data = (void*)NULL;

	module_put(THIS_MODULE);

	return 0;
}

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	const size_t nBufSize = (g_cchDeviceName > (size_t)(*ppos)) ?
			min(count, g_cchDeviceName - (size_t)(*ppos)) : 0;

	/* End of buffer, exit */
	if (0 == nBufSize)
		return 0;

	if (0 != copy_to_user(buf, g_szDeviceName + (*ppos), nBufSize)) {
		/* Failed to copy all the data, exit */
		DbgOut((KERN_ERR "tspdrv: copy_to_user failed.\n"));
		return 0;
	}

	/* Update file position and return copied buffer size */
	*ppos += nBufSize;
	return nBufSize;
}

static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int i = 0;

	*ppos = 0;  /* file position not used, always set to 0 */

	/*
	 * Prevent unauthorized caller to write data.
	 * TouchSense service is the only valid caller.
	 */
	if (file->private_data != (void*)TSPDRV_MAGIC_NUMBER) {
		DbgOut((KERN_ERR "tspdrv: unauthorized write.\n"));
		return 0;
	}

	/* Copy immediately the input buffer */
	if (0 != copy_from_user(g_cWriteBuffer, buf, count)) {
		/* Failed to copy all the data, exit */
		DbgOut((KERN_ERR "tspdrv: copy_from_user failed.\n"));
		return 0;
	}

	/* Check buffer size */
	if ((count <= SPI_HEADER_SIZE) || (count > SPI_BUFFER_SIZE)) {
		DbgOut((KERN_ERR "tspdrv: invalid write buffer size.\n"));
		return 0;
	}

	while (i < count) {
		int nIndexFreeBuffer;   /* initialized below */

		samples_buffer* pInputBuffer = (samples_buffer*)(&g_cWriteBuffer[i]);

		if ((i + SPI_HEADER_SIZE) >= count) {
			/*
			 * Index is about to go beyond the buffer size.
			 * (Should never happen).
			 */
			DbgOut((KERN_EMERG "tspdrv: invalid buffer index.\n"));
		}

		/* Check bit depth */
		if (8 != pInputBuffer->nBitDepth) {
			DbgOut((KERN_WARNING "tspdrv: invalid bit depth. Use default value (8).\n"));
		}

		/* The above code not valid if SPI header size is not 3 */
#if (SPI_HEADER_SIZE != 3)
#error "SPI_HEADER_SIZE expected to be 3"
#endif

		/* Check buffer size */
		if ((i + SPI_HEADER_SIZE + pInputBuffer->nBufferSize) > count) {
			/*
			** Index is about to go beyond the buffer size.
			** (Should never happen).
			*/
			DbgOut((KERN_EMERG "tspdrv: invalid data size.\n"));
		}

		/* Check actuator index */
		if (NUM_ACTUATORS <= pInputBuffer->nActuatorIndex) {
			DbgOut((KERN_ERR "tspdrv: invalid actuator index.\n"));
			i += (SPI_HEADER_SIZE + pInputBuffer->nBufferSize);
			continue;
		}

		if (0 == g_SamplesBuffer[pInputBuffer->nActuatorIndex].actuatorSamples[0].nBufferSize) {
			nIndexFreeBuffer = 0;
		}
		else if (0 == g_SamplesBuffer[pInputBuffer->nActuatorIndex].actuatorSamples[1].nBufferSize) {
			nIndexFreeBuffer = 1;
		}
		else {
			/* No room to store new samples  */
			DbgOut((KERN_ERR "tspdrv: no room to store new samples.\n"));
			return 0;
		}

		/* Store the data in the free buffer of the given actuator */
		memcpy(&(g_SamplesBuffer[pInputBuffer->nActuatorIndex].actuatorSamples[nIndexFreeBuffer]), &g_cWriteBuffer[i], (SPI_HEADER_SIZE + pInputBuffer->nBufferSize));

		/* If the no buffer is playing,prepare to play
		 * g_SamplesBuffer[pInputBuffer->nActuatorIndex]
		 * .actuatorSamples[nIndexFreeBuffer]
		 */
		if ( -1 == g_SamplesBuffer[pInputBuffer->nActuatorIndex].nIndexPlayingBuffer) {
			g_SamplesBuffer[pInputBuffer->nActuatorIndex].nIndexPlayingBuffer = nIndexFreeBuffer;
			g_SamplesBuffer[pInputBuffer->nActuatorIndex].nIndexOutputValue = 0;
		}

		/* Increment buffer index */
		i += (SPI_HEADER_SIZE + pInputBuffer->nBufferSize);
	}

#ifdef QA_TEST
	g_nForceLog[g_nForceLogIndex++] = g_cSPIBuffer[0];
	if (g_nForceLogIndex >= FORCE_LOG_BUFFER_SIZE) {
		for (i = 0; i < FORCE_LOG_BUFFER_SIZE; i++) {
			printk("<6>%d\t%d\n", g_nTime, g_nForceLog[i]);
			g_nTime += TIME_INCREMENT;
		}
		g_nForceLogIndex = 0;
	}
#endif

	/* Start the timer after receiving new output force */
	g_bIsPlaying = true;
	VibeOSKernelLinuxStartTimer();

	return count;
}

#if HAVE_UNLOCKED_IOCTL
static long unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#else
static int ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
#endif
{
#ifdef QA_TEST
	int i;
#endif

	switch (cmd) {
	case TSPDRV_STOP_KERNEL_TIMER:
		/*
		 * As we send one sample ahead of time, we need to finish
		 * playing the last samplebefore stopping the timer.
		 * So we just set a flag here.
		 */
		if (true == g_bIsPlaying)
			g_bStopRequested = true;

#ifdef VIBEOSKERNELPROCESSDATA
		/* Last data processing to disable amp and stop timer */
		VibeOSKernelProcessData(NULL);
#endif

#ifdef QA_TEST
		if (g_nForceLogIndex) {
			for (i = 0; i < g_nForceLogIndex; i++) {
				printk(KERN_INFO "%d\t%d\n", g_nTime,
						g_nForceLog[i]);
				g_nTime += TIME_INCREMENT;
			}
		}
		g_nTime = 0;
		g_nForceLogIndex = 0;
#endif
		break;

	case TSPDRV_MAGIC_NUMBER:
		file->private_data = (void*)TSPDRV_MAGIC_NUMBER;
		break;

	case TSPDRV_ENABLE_AMP:
		ImmVibeSPI_ForceOut_AmpEnable(arg);
		DbgRecorderReset((arg));
		DbgRecord((arg,";TSPDRV_ENABLE_AMP\n"));
		break;

	case TSPDRV_DISABLE_AMP:
		/* Small fix for now to handle proper combination of
		 * TSPDRV_STOP_KERNEL_TIMER and TSPDRV_DISABLE_AMP together
		 * If a stop was requested, ignore the request
		 * as the amp will be disabled by the timer proc
		 * when it's ready
		 */
		if (!g_bStopRequested) {
			ImmVibeSPI_ForceOut_AmpDisable(arg);
		}
		break;

	case TSPDRV_GET_NUM_ACTUATORS:
		return NUM_ACTUATORS;
	}

	return 0;
}

static int suspend(struct platform_device *pdev, pm_message_t state)
{
	if (g_bIsPlaying) {
	       	DbgOut((KERN_INFO "tspdrv: can't suspend, still playing effects.\n"));
	       	return -EBUSY;
	}
	else {
		DbgOut((KERN_INFO "tspdrv: suspend.\n"));
		return 0;
	}
}

static int resume(struct platform_device *pdev)
{
	DbgOut((KERN_INFO "tspdrv: resume.\n"));

	return 0;   /* can resume */
}

static void platform_release(struct device *dev)
{
	DbgOut((KERN_INFO "tspdrv: platform_release.\n"));
}

module_init(tspdrv_init);
module_exit(tspdrv_exit);

/* Module info */
MODULE_AUTHOR("Immersion Corporation");
MODULE_DESCRIPTION("TouchSense Kernel Module");
MODULE_LICENSE("GPL v2");
