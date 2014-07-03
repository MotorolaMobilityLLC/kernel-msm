#if !defined UTILF_H
#define UTILF_H

#include <linux/types.h>

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
	__attribute__((format(__printf__, 4, 5)));


#endif /* if !defined UTILF_H */
