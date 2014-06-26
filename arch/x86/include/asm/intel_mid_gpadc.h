#ifndef __INTEL_MID_GPADC_H__
#define __INTEL_MID_GPADC_H__

struct intel_mid_gpadc_platform_data {
	unsigned long intr;
};

#define CH_NEED_VREF		(1 << 8)
#define CH_NEED_VCALIB		(1 << 9)
#define CH_NEED_ICALIB		(1 << 10)

int intel_mid_gpadc_gsmpulse_sample(int *vol, int *cur);
int intel_mid_gpadc_sample(void *handle, int sample_count, ...);
int get_gpadc_sample(void *handle, int sample_count, int *buffer);
void intel_mid_gpadc_free(void *handle);
void *intel_mid_gpadc_alloc(int count, ...);
void *gpadc_alloc_channels(int count, int *channel_info);
#endif

