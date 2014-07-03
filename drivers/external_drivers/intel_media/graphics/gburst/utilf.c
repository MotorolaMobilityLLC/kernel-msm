#include <linux/kernel.h>
#include <linux/module.h>

#include "utilf.h"


/**
 * ut_isnprintf -- Like snprintf, except also accepts initial buffer index.
 * @ix: Index within the buffer at which to begin output.
 * @pbuf: Pointer to output buffer
 * @bdim: Dimension of output buffer
 * @fmt: snprintf-style format string.
 * @...:  snprintf-style variable argument list.
 *
 * Except when input ix == -1, the output buffer is guaranteed to be
 * null-terminated, even if underlying functions do not do so.
 *
 * If the specified initial index is negative, then an immediate error
 * return is done.  One can therefore make multiple sequential calls to this
 * function without having to check the return value until the very end.
 * Example:
 *      char sbuf[some_size];
 *      int ix = 0;
 *      ix = ut_isnprintf(ix, sbuf, sizeof(sbuf), "%s", arga);
 *      ix = ut_isnprintf(ix, sbuf, sizeof(sbuf), "%s", argb);
 *      if (ix < 0)
 *              error_generic();
 *      if (ix >= sizeof(sbuf)
 *              error_buffer_too_small();
 * Function return value:
 * -1 if error:
 * --  Underlying vsnprintf call gives error return.  Should not happen.
 * --  Input parameter ix is negative.
 * otherwise, the return value is the number of characters which would
 * be generated for the given input, excluding the trailing '\0', as
 * per ISO C99 (i.e., this may exceed the actual buffer length).
 */
int ut_isnprintf(int ix, char *pbuf, size_t bdim, const char *fmt, ...)
{
	va_list ap;
	int nchrs;
	int ii;

	if (ix < 0)
		return -1;

	/**
	 * If already past end of buffer, pass dimension of 0 to vsnprintf
	 * so it will tell us how many additional characters are required.
	 */
	if (ix > bdim)
		ii = bdim;
	else
		ii = ix;

	va_start(ap, fmt);
	nchrs = vsnprintf(pbuf+ii, bdim-ii, fmt, ap);
	va_end(ap);

	/**
	 * Check in case vsnprintf returns an error value (not thought to
	 * happen).
	 */
	if (nchrs < 0)
		pbuf[ii] = '\0';
	else {
		nchrs += ix;
		if (nchrs < bdim)
			pbuf[nchrs] = '\0';
		else
			pbuf[bdim-1] = '\0';
	}

	return nchrs;
}
