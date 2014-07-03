/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

	Copyright(c) 2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:

  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  95054

  BSD LICENSE

	Copyright(c) 2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#ifndef _OTM_HDMI_TYPES_H
#define _OTM_HDMI_TYPES_H

/**
 *	typedef enum otm_hdmi_ret_t - OTM HDMI module return code definition
 *	OTM_HDMI_SUCCESS:	Function executed without errors.
 *	OTM_HDMI_ERR_NO_MEMORY:	Could not allocate memory.
 *	OTM_HDMI_ERR_FAILED:	This is a generic error code  means that a
 *                              system call or call to some other software
 *                              external to the driver.
 *	OTM_HDMI_ERR_INTERNAL:	A condition that "should not be possible" was
 *                              detected within the driver. This generally
 *                              means there is nothing the application can do
 *                              to correct the problem.
 */
typedef enum {
	OTM_HDMI_SUCCESS = 0,
	/**<
	Function executed without errors
	*/
	OTM_HDMI_ERR_INVAL = 0x01,
	/**<
	An invalid argument was passed.
	*/
	OTM_HDMI_ERR_BUSY = 0x02,
	/**<
	An operation could not be completed because a needed resource is in use.
	*/
	OTM_HDMI_ERR_DISPLAY = 0x03,
	/**<
	An invalid display ID was passed.
	*/
	OTM_HDMI_ERR_SURFACE = 0x04,
	/**<
	An invalid surface ID, or the ID of a surface that is not
	appropriate for the requested operation, was passed.
	*/
	OTM_HDMI_ERR_COMMAND = 0x05,
	/**<
	An internal command processing error occurred
	*/
	OTM_HDMI_ERR_NULL_ARG = 0x06,
	/**<
	A required argument was missing.  Either a NULL pointer or a count
	of 0 was passed for a required argument.
	*/
	OTM_HDMI_ERR_NO_MEMORY = 0x07,
	/**<
	Could not allocate memory.
	*/
	OTM_HDMI_ERR_FAILED = 0x08,
	/**<
	This is a generic error code that generally means that a system
	call or call to some other software external to the driver
	returned a failure code.
	*/
	OTM_HDMI_ERR_INTERNAL = 0x09,
	/**<
	A condition that "should not be possible" was detected within the
	driver.  This generally means there is nothing the application can
	do to correct the problem.
	*/
	OTM_HDMI_ERR_NOT_IMPL = 0x0a,
	/**<
	The function is not currently implemented for the target chip.
	*/
	OTM_HDMI_ERR_MAPPED = 0x0b,
	/**<
	Operation not permitted on the mapped surface.
	*/
	OTM_HDMI_ERR_NO_INIT = 0x0c,
	/**<
	A GDL function was called without a preceding call to gdl_init().
	*/
	OTM_HDMI_ERR_NO_HW_SUPPORT = 0x0d,
	/**<
	The target chip does not support the requested function.  Examples:
	- A graphics rendering option is not supported by the graphics core
	  in the target chip.
	- A plane or port driver does not support a requested attribute.
	- An attempt was made to request the attribute list from a port
	  driver that does not support any attributes.
	*/
	OTM_HDMI_ERR_INVAL_PF = 0x0e,
	/**<
	An unknown pixel format, or a pixel format not supported by the
	attempted operation, was passed.
	*/
	OTM_HDMI_ERR_INVAL_RECT = 0x0f,
	/**<
	An invalid argument of type #gdl_rectangle_t was passed to the function.
	*/
	OTM_HDMI_ERR_ATTR_ID = 0x10,
	/**<
	An undefined ID was specified for a plane attribute or a port
	driver attribute.
	*/
	OTM_HDMI_ERR_ATTR_NO_SUPPORT = 0x11,
	/**<
	An unsupported ID was specified for a plane attribute or a port
	driver attribute.
	*/
	OTM_HDMI_ERR_ATTR_READONLY = 0x12,
	/**<
	An attempt was made to set the value of a read-only plane attribute
	or port driver attribute.
	*/
	OTM_HDMI_ERR_ATTR_VALUE = 0x13,
	/**<
	An invalid value was specified for a plane attribute or a port
	driver attribute.
	*/
	OTM_HDMI_ERR_PLANE_CONFLICT = 0x14,
	/**<
	An attempt was made to change the display mode to a resolution too
	small to accommodate all of the currently enabled planes at their
	current positions on the display. Move/shrink the affected planes first.
	*/
	OTM_HDMI_ERR_DISPLAY_CONFLICT = 0x15,
	/**<
	An attempt was made to change either display resolution or plane
	size/origin, such that part/all of the plane will no longer be on the
	display.
	- If the display resolution is being reduced, change plane size/origin
	  first.
	- If plane size is being increased, increase the display resolution
	  first, or reposition the plane.
	- If plane origin is being changed, make sure you have picked an
	  appropriate origin given the current plane size and display
	  resolution.
	*/
	OTM_HDMI_ERR_TIMEOUT = 0x16,
	/**<
	The requested timeout period occurred before the requested
	operation trigger occurred.
	*/
	OTM_HDMI_ERR_MISSING_BEGIN = 0x17,
	 /**<
	 An attempt was made to set a plane attribute without first calling
	 gdl_config_begin().
	 */
	OTM_HDMI_ERR_PLANE_ID = 0x18,
	/**<
	An invalid plane ID was passed.  The ID is undefined, the plane is not
	supported by the target chip, or the plane is not supported by the
	called function.
	*/
	OTM_HDMI_ERR_INVAL_PTR = 0x19,
	/**<
	On Linux, a copy between user and kernel space failed.  This
	probably indicates an invalid user space (argument) pointer.
	*/

	OTM_HDMI_ERR_INVAL_HEAP = 0x1a,
	/**<
	An invalid heap was passed for addition or removal. Attempt
	to add overlaping heaps will cause this error too.
	*/

	OTM_HDMI_ERR_HEAP_IN_USE = 0x1b,
	/**<
	Heap removal was attempted while at least one surface was allocated
	from that heap.
	*/

	OTM_HDMI_ERR_INVAL_CALLBACK = 0x1c,
	/**<
	Invalid callback (null) was passed to gdl_event_register() function
	*/

	OTM_HDMI_ERR_SCALING_POLICY = 0x1d,
	/**<
	A single scaling policy is required and was not specified for the
	unsupported for the specified display ID.
	*/

	OTM_HDMI_ERR_INVAL_EVENT = 0x1e,
	/**<
	Invalid event was passed to functions expecting #gdl_app_event_t.
	*/

	OTM_HDMI_ERR_INVAL_IOCTL = 0x1f,
	/**<
	Invalid IOCTL request was sent to kernel module
	*/
	OTM_HDMI_ERR_SCHED_IN_ATOMIC = 0x20,
	/**<
	Scheduling was attempted while being in atomic context.
	*/
	OTM_HDMI_ERR_MMAP = 0x21,
	/**<
	Memory mapping failed
	*/
	OTM_HDMI_ERR_HDCP = 0x22,
	/**<
	HDCP failure
	*/
	OTM_HDMI_ERR_CONFIG = 0x23,
	/**<
	Platform config file error: either a required entry in the
	platform configuration file is missing, or its entry is invalid.
	*/
	OTM_HDMI_ERR_HDMI_AUDIO_PLAYBACK = 0x24,
	/**<
	HDMI Audio start / stop / set buffer / set format command was
	initiated at the wrong time.
	*/
	OTM_HDMI_ERR_HDMI_AUDIO_BUFFER_FULL = 0x25,
	/**<
	Given data does not fit in the internal buffer
	*/
	OTM_HDMI_ERR_PLANE_ORIGIN_ODD = 0x26,
	/**<
	In interlaced display modes, active planes must be configured with
	their origins on even display lines. This error is returned when:
	- in a progressive display mode: an attempt is made to change to an
	  interlaced display mode while there is an active plane does not
	  meet this requirement.
	- in an interlaced display mode:
	   - an attempt is made to reconfigure an active plane's origin
	     to an odd line number, OR
	   - an attempt is made to activate (by flipping a surface to) a
	     plane that doesn't meet this requirement.
	*/
	OTM_HDMI_ERR_PLANE_HEIGHT_ODD = 0x27,
	/**<
	In interlaced display modes, active planes must be configured with
	their even heights. This error is returned when:
	- in a progressive display mode: an attempt is made to change to an
	  interlaced display mode while there is an active plane does not
	  meet this requirement.
	- in an interlaced display mode:
	   - an attempt is made to reconfigure an active plane's height
	     to an odd value, OR
	   - an attempt is made to activate (by flipping a surface to) a
	     plane that doesn't meet this requirement.
	*/
	OTM_HDMI_ERR_HANDLE = 0x28,
	/**<
	Handle is not valid.
	*/
	OTM_HDMI_ERR_TVMODE_UNDEFINED = 0x29,
	/**<
	Display has undefined tv mode set on it.
	*/
	OTM_HDMI_ERR_PREMULT_CONFLICT = 0x2a,
	/**<
	An attempt was made to enable the #OTM_HDMI_PLANE_ALPHA_PREMULT
	attribute and one of the following incompatible features at the same
	time:
	- Chroma keying on the same plane
	  (#OTM_HDMI_PLANE_CHROMA_KEY_SRC_ENABLE set to #OTM_HDMI_TRUE).
	- Gamma removal on the same plane (#OTM_HDMI_PLANE_REVERSE_GAMMA_TYPE
	  set to a value other than #OTM_HDMI_GAMMA_LINEAR.
	- color space conversion (the value of the plane's

	  space of the display to which it is connected).
	- a non-RGB pixel format.
	*/

	OTM_HDMI_ERR_SUSPENDED = 0x2b,
	/**<
	An attempt was made to execute a command while the driver was in a
	suspended mode. During the suspended mode driver is in a low-power
	state and no access to hardware is allowed.
	*/

	OTM_HDMI_ERR_STEREO_PLANE = 0x2c,
	/**<
	An attempt was made to stereo-flip to a plane unlinked to a right view
	while a two-plane stereo display mode is in effect.
	*/

	OTM_HDMI_ERR_CE4100_3D_ORIGIN = 0x2d,
	/**<
	On the CE4100, the origin of a plane's destination rectangle cannot
	exceed 922 when OTM_HDMI_STEREO_FRAME_PACKING_2 stereo frame format is
	in use.
	*/

	OTM_HDMI_ERR_HDCP_KSV_INVALID = 0x2e,
	/**<
	HDCP invalid KSV
	*/
	OTM_HDMI_ERR_HDCP_KSV_REVOKED = 0x2f,
	/**<
	HDCP revoked KSV
	*/
	OTM_HDMI_ERR_HDCP_NO_ACK = 0x30,
	/**<
	HDCP I2C timeout when receiving R'
	*/
	OTM_HDMI_ERR_HDCP_LINK_INTEGRITY = 0x31,
	/**<
	HDCP R != R'
	*/

	OTM_HDMI_ERR_PERM = 0x32,
	/**<
	Callers permissions are insufficient to perform a requested action.
	*/

	/**********************************************************************
	 ATTENTION!!: WHEN ADDING AN ERROR CODE MAKE SURE TO:
	 - Search for a value marked "Currently unused" in the list above
	   before adding a new value at the end.
	 - Include inline (doxygen) documentation for the new error.
	 - Add the new error to _error_string() in debug.c
	**********************************************************************/
} otm_hdmi_ret_t;

/* ----------------------------------------------------------------------
 *                 D I S P L A Y   M O D E
 * ----------------------------------------------------------------------
 */

/**
 *  Refresh rates for TV mode definitions.
 *
 *  Refresh rate is the number of times the display is updated per second.
 *  This is the number of frames per second for progressive display modes;
 *  the number of fields (half the number of frames) per second for interlaced
 *  display modes.
 *
*/
typedef enum {
	OTM_HDMI_REFRESH_23_98, /* 23.98... (24/1.001)    */
	OTM_HDMI_REFRESH_24,	/* 24                     */
	OTM_HDMI_REFRESH_25,	/* 25                     */
	OTM_HDMI_REFRESH_29_97, /* 29.97... (30/1.001)    */
	OTM_HDMI_REFRESH_30,	/* 30 - DEPRECATED: This value is normally only
				   used on computer systems and should be used
				   with care, if at all. The corresponding TV
				   rate is 30/(1.001) (see
				   #OTM_HDMI_REFRESH_29_97). */
	OTM_HDMI_REFRESH_50,	/* 50                     */
	OTM_HDMI_REFRESH_59_94, /* 59.94... (60/1.001)    */
	OTM_HDMI_REFRESH_60,	/* 60 - DEPRECATED: This value is normally only
				   used on computer systems and should be used
				   with care, if at all. The corresponding TV
				   rate is 60/(1.001) (see
				   #OTM_HDMI_REFRESH_59_94). */
	OTM_HDMI_REFRESH_48,	/* 48 - DEPRECATED: This value is normally only
				   used on HDMI output with special sink device
				   and should be used with care, if at all. */
	OTM_HDMI_REFRESH_47_96, /* 47.96... (48/1.001)   */
	OTM_HDMI_REFRESH_NONE,  /* Indicates that mode is not set */
	OTM_HDMI_REFRESH_USER_DEFINED
				/* External (non-Intel) port drivers may define
				   additional refresh rates that the support.
				   Their IDs must be numbered starting at this
				   value. */
} otm_hdmi_refresh_t;

/**
 *  This enumeration is used to specify a stereo (3D) video format.  The SOCs
 *  on which each format is supported are specified within square brackets.
 *
 *  Format names ending in "_2" indicate that the format requires the use of
 *  two UPPs, one for the left view and one for the right. The
 *
 *  to reference the left view plane in order to link them together.
 *
 *  OTM_HDMI_STEREO_NONE
 *     Indicates a mono display mode (no stereo format is in use).
 *
 *  OTM_HDMI_STEREO_FRAME_PACKING_2
 *     Frame packing format implemented with 2 planes per stream.
 *
 *  OTM_HDMI_STEREO_FRAME_PACKING [CE4200-B and above]
 *     Single-plane frame packing format.
 *
 *  OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2
 *     Side-by-side format with the horizontal axis subsampled by half,
 *     implemented with 2 planes per stream.
 *     NOTE: Planes should be configured and buffers for graphics
 *     allocated at half horizontal resolution. The TV set is responsible for
 *     scaling the blended image horizontally by 2.
 *
 *  OTM_HDMI_STEREO_TOP_BOTTOM_HALF_2
 *     Top-and-bottom format with the vertical axis subsampled by half,
 *     implemented with 2 planes per stream.
 *     NOTE: Planes should be configured and buffers for graphics
 *     allocated at half vertical resolution. The TV set is responsible for
 *     scaling the blended image vertically by 2.
 *
 *  OTM_HDMI_STEREO_FRAME_SEQUENTIAL
 *     Frame sequential format, a format used internally in some Digital TV
 *     sets for direct output to the panel. NOTE: in order to use Frame
 *     Sequential format, the HDMI port driver must be loaded with the dtv=1
 *     command line argument.
*/
typedef enum {
	OTM_HDMI_STEREO_NONE = 0xabcdef01,
	OTM_HDMI_STEREO_FRAME_PACKING_2,
	OTM_HDMI_STEREO_FRAME_PACKING,
	OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2,
	OTM_HDMI_STEREO_TOP_BOTTOM_HALF_2,
	OTM_HDMI_STEREO_FRAME_SEQUENTIAL,
} otm_hdmi_stereo_t;

/*
 * Scaling type flags to passdown to the IP layer
 */
typedef enum {
	OTM_HDMI_SCALE_NONE = 0,   /* Unmodified timing (display or
					software can still scale) */
	OTM_HDMI_SCALE_FULLSCREEN, /* Full screen, ignore aspect */
	OTM_HDMI_SCALE_CENTER,     /* Centered, no scaling */
	OTM_HDMI_SCALE_ASPECT,     /* Full screen, preserve aspect */
} otm_hdmi_scale_t;

#endif /* _OTM_HDMI_TYPES_H */
