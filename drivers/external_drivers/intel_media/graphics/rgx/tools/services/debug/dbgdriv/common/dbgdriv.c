/*************************************************************************/ /*!
@File
@Title          Debug Driver
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    32 Bit kernel mode debug driver
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/
#if defined(UNDER_CE)
#include <windows.h>
#include <ceddk.h>

#else
#if defined(_WIN32)
#pragma  warning(disable:4201)
#pragma  warning(disable:4214)
#pragma  warning(disable:4115)
#pragma  warning(disable:4514)


#include <ntddk.h>
#include <windef.h>
#include <winerror.h>
#endif /* _WIN32 */
#endif /* UNDER_CE */

#ifdef LINUX
#include <linux/string.h>
#endif

#if defined (__QNXNTO__)
#include <string.h>
#endif

#include "img_types.h"
#include "pvr_debug.h"
#include "dbgdrvif.h"
#include "dbgdriv.h"
#include "hotkey.h"
#include "hostfunc.h"

#ifdef _WIN32
#pragma  warning(default:4214)
#pragma  warning(default:4115)
#endif /* _WIN32 */



typedef struct _DBG_LASTFRAME_BUFFER_
{
	PDBG_STREAM	psStream;
	IMG_UINT8 ui8Buffer[LAST_FRAME_BUF_SIZE];
	IMG_UINT32		ui32BufLen;
	struct _DBG_LASTFRAME_BUFFER_	*psNext;
} *PDBG_LASTFRAME_BUFFER;

/*
	Common control structure (don't duplicate control in main stream
	and init phase stream).
*/
typedef struct _DBG_STREAM_CONTROL_
{
	IMG_BOOL   bInitPhaseComplete;		/*!< init phase has finished */
	IMG_UINT32 ui32Flags;			/*!< flags (see DEBUG_FLAGS above) */

	IMG_UINT32 ui32CapMode;			/*!< capturing mode framed/hot key */
	IMG_UINT32 ui32OutMode;			/*!< output mode, e.g. files */
	IMG_UINT32 ui32DebugLevel;
	IMG_UINT32 ui32DefaultMode;
	IMG_UINT32 ui32Start;			/*!< first capture frame */
	IMG_UINT32 ui32End;				/*!< last frame */
	IMG_UINT32 ui32Current;			/*!< current frame */
	IMG_UINT32 ui32SampleRate;		/*!< capture frequency */
	IMG_UINT32 ui32Reserved;
} DBG_STREAM_CONTROL;
/*
	Per-buffer control structure.
*/
typedef struct _DBG_STREAM_
{
	struct _DBG_STREAM_ *psNext;
	struct _DBG_STREAM_ *psInitStream;
	DBG_STREAM_CONTROL *psCtrl;
	IMG_BOOL   bCircularAllowed;
	IMG_PVOID  pvBase;
	IMG_UINT32 ui32Size;
	IMG_UINT32 ui32RPtr;
	IMG_UINT32 ui32WPtr;
	IMG_UINT32 ui32DataWritten;
	IMG_UINT32 ui32Marker;			/*!< marker for file splitting */
	IMG_UINT32 ui32InitPhaseWOff;	/*!< snapshot offset for init phase end for follow-on pdump */
	IMG_CHAR szName[30];		/* Give this a size, some compilers don't like [] */
} DBG_STREAM;


/******************************************************************************
 Global vars
******************************************************************************/
static PDBG_STREAM	g_psStreamList = 0;
static PDBG_LASTFRAME_BUFFER	g_psLFBufferList;
static PDBG_LASTFRAME_BUFFER	g_psLFDeinitBufferList;

static IMG_UINT32		g_ui32LOff = 0;
static IMG_UINT32		g_ui32Line = 0;
static IMG_UINT32		g_ui32MonoLines = 25;

static IMG_BOOL			g_bHotkeyMiddump = IMG_FALSE;
static IMG_UINT32		g_ui32HotkeyMiddumpStart = 0xffffffff;
static IMG_UINT32		g_ui32HotkeyMiddumpEnd = 0xffffffff;

IMG_VOID *				g_pvAPIMutex=IMG_NULL;

extern IMG_UINT32		g_ui32HotKeyFrame;
extern IMG_BOOL			g_bHotKeyPressed;
extern IMG_BOOL			g_bHotKeyRegistered;


IMG_UINT32 SpaceInStream(PDBG_STREAM psStream);
IMG_BOOL ExpandStreamBuffer(PDBG_STREAM psStream, IMG_UINT32 ui32NewSize);
PDBG_LASTFRAME_BUFFER FindLFBufFromList(PDBG_STREAM psStream, PDBG_LASTFRAME_BUFFER	psLFBuffer);

/***************************************************************************
 Declare kernel mode service table.
***************************************************************************/
DBGKM_SERVICE_TABLE g_sDBGKMServices =
{
	sizeof (DBGKM_SERVICE_TABLE),
	ExtDBGDrivCreateStream,
	ExtDBGDrivDestroyStream,
	ExtDBGDrivFindStream,
	ExtDBGDrivWriteString,
	ExtDBGDrivReadString,
	ExtDBGDrivWrite,
	ExtDBGDrivRead,
	ExtDBGDrivSetCaptureMode,
	ExtDBGDrivSetOutputMode,
	ExtDBGDrivSetDebugLevel,
	ExtDBGDrivSetFrame,
	ExtDBGDrivGetFrame,
	ExtDBGDrivOverrideMode,
	ExtDBGDrivDefaultMode,
	ExtDBGDrivWrite2,
	ExtDBGDrivWriteStringCM,
	ExtDBGDrivWriteCM,
	ExtDBGDrivSetMarker,
	ExtDBGDrivGetMarker,
	ExtDBGDrivStartInitPhase,
	ExtDBGDrivStopInitPhase,
	ExtDBGDrivIsInitPhase,
	ExtDBGDrivIsCaptureFrame,
	ExtDBGDrivWriteLF,
	ExtDBGDrivReadLF,
	ExtDBGDrivGetStreamOffset,
	ExtDBGDrivSetStreamOffset,
	ExtDBGDrivIsLastCaptureFrame,
	ExtDBGDrivWaitForEvent,
	ExtDBGDrivSetConnectNotifier,
	ExtDBGDrivWritePersist,
	ExtDBGDrivGetCtrlState
};

/* Forward declarations */

IMG_VOID * IMG_CALLCONV DBGDrivCreateStream(IMG_CHAR *pszName, IMG_UINT32 ii32CapMode, IMG_UINT32 ui32OutMode, IMG_UINT32 ui32Flags, IMG_UINT32 ui32Pages);
IMG_VOID   IMG_CALLCONV DBGDrivDestroyStream(PDBG_STREAM psStream);
IMG_VOID * IMG_CALLCONV DBGDrivFindStream(IMG_CHAR * pszName, IMG_BOOL bResetStream);
IMG_UINT32 IMG_CALLCONV DBGDrivWriteString(PDBG_STREAM psStream,IMG_CHAR * pszString,IMG_UINT32 ui32Level);
IMG_UINT32 IMG_CALLCONV DBGDrivReadString(PDBG_STREAM psStream,IMG_CHAR * pszString,IMG_UINT32 ui32Limit);
IMG_UINT32 IMG_CALLCONV DBGDrivWrite(PDBG_STREAM psStream,IMG_UINT8 *pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level);
IMG_UINT32 IMG_CALLCONV DBGDrivRead(PDBG_STREAM psStream, IMG_BOOL bReadInitBuffer, IMG_UINT32 ui32OutBufferSize,IMG_UINT8 *pui8OutBuf);
IMG_VOID   IMG_CALLCONV DBGDrivSetCaptureMode(PDBG_STREAM psStream,IMG_UINT32 ui32Mode,IMG_UINT32 ui32Start,IMG_UINT32 ui32Stop,IMG_UINT32 ui32SampleRate);
IMG_VOID   IMG_CALLCONV DBGDrivSetOutputMode(PDBG_STREAM psStream,IMG_UINT32 ui32OutMode);
IMG_VOID   IMG_CALLCONV DBGDrivSetDebugLevel(PDBG_STREAM psStream,IMG_UINT32 ui32DebugLevel);
IMG_VOID   IMG_CALLCONV DBGDrivSetFrame(PDBG_STREAM psStream,IMG_UINT32 ui32Frame);
IMG_UINT32 IMG_CALLCONV DBGDrivGetFrame(PDBG_STREAM psStream);
IMG_VOID   IMG_CALLCONV DBGDrivOverrideMode(PDBG_STREAM psStream,IMG_UINT32 ui32Mode);
IMG_VOID   IMG_CALLCONV DBGDrivDefaultMode(PDBG_STREAM psStream);
IMG_UINT32 IMG_CALLCONV DBGDrivWrite2(PDBG_STREAM psStream,IMG_UINT8 *pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level);
IMG_UINT32 IMG_CALLCONV DBGDrivWriteStringCM(PDBG_STREAM psStream,IMG_CHAR * pszString,IMG_UINT32 ui32Level);
IMG_UINT32 IMG_CALLCONV DBGDrivWriteCM(PDBG_STREAM psStream,IMG_UINT8 *pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level);
IMG_VOID   IMG_CALLCONV DBGDrivSetMarker(PDBG_STREAM psStream, IMG_UINT32 ui32Marker);
IMG_UINT32 IMG_CALLCONV DBGDrivGetMarker(PDBG_STREAM psStream);
IMG_VOID   IMG_CALLCONV DBGDrivStartInitPhase(PDBG_STREAM psStream);
IMG_VOID   IMG_CALLCONV DBGDrivStopInitPhase(PDBG_STREAM psStream);
IMG_BOOL   IMG_CALLCONV DBGDrivIsInitPhase(PDBG_STREAM psStream);IMG_BOOL   IMG_CALLCONV DBGDrivIsLastCaptureFrame(PDBG_STREAM psStream);
IMG_UINT32 IMG_CALLCONV DBGDrivWriteLF(PDBG_STREAM psStream, IMG_UINT8 *pui8InBuf, IMG_UINT32 ui32InBuffSize, IMG_UINT32 ui32Level, IMG_UINT32 ui32Flags);
IMG_UINT32 IMG_CALLCONV DBGDrivReadLF(PDBG_STREAM psStream, IMG_UINT32 ui32OutBuffSize, IMG_UINT8 *pui8OutBuf);
IMG_UINT32 IMG_CALLCONV DBGDrivGetStreamOffset(PDBG_STREAM psStream);
IMG_VOID   IMG_CALLCONV DBGDrivSetStreamOffset(PDBG_STREAM psStream, IMG_UINT32 ui32StreamOffset);
IMG_BOOL   IMG_CALLCONV DBGDrivIsCaptureFrame(PDBG_STREAM psStream, IMG_BOOL bCheckPreviousFrame);
IMG_VOID   IMG_CALLCONV DBGDrivWaitForEvent(DBG_EVENT eEvent);
//ExtDBGDrivWaitForEvent
//ExtDBGDrivSetConnectNotifier
//ExtDBGDrivWritePersist
IMG_UINT32 IMG_CALLCONV DBGDrivGetCtrlState(PDBG_STREAM psStream, IMG_UINT32 ui32StateID);

IMG_VOID DestroyAllStreams(IMG_VOID);

/* Static function declarations */
static IMG_UINT32 DBGDrivWritePersist(PDBG_STREAM psMainStream,IMG_UINT8 * pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level);
static IMG_VOID InvalidateAllStreams(IMG_VOID);

/*****************************************************************************
 Code
*****************************************************************************/
DBGKM_CONNECT_NOTIFIER g_fnDBGKMNotifier;

/*!
 @name		ExtDBGDrivSetConnectNotifier
 @brief		Registers one or more services callback functions which are called on events in the dbg driver
 @param		fn_notifier - services callbacks
 @return	none
 */
IMG_VOID IMG_CALLCONV ExtDBGDrivSetConnectNotifier(DBGKM_CONNECT_NOTIFIER fn_notifier)
{
	/* Set the callback function which enables the debug driver to
	 * communicate to services KM when pdump is connected.
	 */
	g_fnDBGKMNotifier = fn_notifier;
}

/*!
 @name	ExtDBGDrivCreateStream
 */
IMG_VOID * IMG_CALLCONV ExtDBGDrivCreateStream(IMG_CHAR *	pszName, IMG_UINT32 ui32CapMode, IMG_UINT32	ui32OutMode, IMG_UINT32 ui32Flags, IMG_UINT32 ui32Size)
{
	IMG_VOID *	pvRet;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	pvRet=DBGDrivCreateStream(pszName, ui32CapMode, ui32OutMode, ui32Flags, ui32Size);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return pvRet;
}

/*!
 @name	ExtDBGDrivDestroyStream
 */
void IMG_CALLCONV ExtDBGDrivDestroyStream(PDBG_STREAM psStream)
{
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	DBGDrivDestroyStream(psStream);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return;
}

/*!
 @name	ExtDBGDrivFindStream
 */
IMG_VOID * IMG_CALLCONV ExtDBGDrivFindStream(IMG_CHAR * pszName, IMG_BOOL bResetStream)
{
	IMG_VOID *	pvRet;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	pvRet=DBGDrivFindStream(pszName, bResetStream);
	if (pvRet != IMG_NULL)
	{
		if(g_fnDBGKMNotifier.pfnConnectNotifier)
		{
			g_fnDBGKMNotifier.pfnConnectNotifier(pszName);
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR, "ExtDBGDrivFindStream: pfnConnectNotifier not initialised"));
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "ExtDBGDrivFindStream: Stream not found"));
	}


	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return pvRet;
}

/*!
 @name	ExtDBGDrivWriteString
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivWriteString(PDBG_STREAM psStream,IMG_CHAR * pszString,IMG_UINT32 ui32Level)
{
	IMG_UINT32	ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret=DBGDrivWriteString(psStream, pszString, ui32Level);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}

/*!
 @name	ExtDBGDrivReadString
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivReadString(PDBG_STREAM psStream,IMG_CHAR * pszString,IMG_UINT32 ui32Limit)
{
	IMG_UINT32 ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret=DBGDrivReadString(psStream, pszString, ui32Limit);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}

/*!
 @name	ExtDBGDrivWrite
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivWrite(PDBG_STREAM psStream,IMG_UINT8 * pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level)
{
	IMG_UINT32	ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret=DBGDrivWrite(psStream, pui8InBuf, ui32InBuffSize, ui32Level);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}

/*!
 @name	ExtDBGDrivRead
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivRead(PDBG_STREAM psStream, IMG_BOOL bReadInitBuffer, IMG_UINT32 ui32OutBuffSize,IMG_UINT8 * pui8OutBuf)
{
	IMG_UINT32 ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret=DBGDrivRead(psStream, bReadInitBuffer, ui32OutBuffSize, pui8OutBuf);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}

/*!
 @name	ExtDBGDrivSetCaptureMode
 */
void IMG_CALLCONV ExtDBGDrivSetCaptureMode(PDBG_STREAM psStream,IMG_UINT32 ui32Mode,IMG_UINT32 ui32Start,IMG_UINT32 ui32End,IMG_UINT32 ui32SampleRate)
{
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	DBGDrivSetCaptureMode(psStream, ui32Mode, ui32Start, ui32End, ui32SampleRate);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return;
}

/*!
 @name	ExtDBGDrivSetOutputMode
 */
void IMG_CALLCONV ExtDBGDrivSetOutputMode(PDBG_STREAM psStream,IMG_UINT32 ui32OutMode)
{
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	DBGDrivSetOutputMode(psStream, ui32OutMode);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return;
}

/*!
 @name	ExtDBGDrivSetDebugLevel
 */
void IMG_CALLCONV ExtDBGDrivSetDebugLevel(PDBG_STREAM psStream,IMG_UINT32 ui32DebugLevel)
{
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	DBGDrivSetDebugLevel(psStream, ui32DebugLevel);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return;
}

/*!
 @name	ExtDBGDrivSetFrame
 */
void IMG_CALLCONV ExtDBGDrivSetFrame(PDBG_STREAM psStream,IMG_UINT32 ui32Frame)
{
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	DBGDrivSetFrame(psStream, ui32Frame);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return;
}

/*!
 @name	ExtDBGDrivGetFrame
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivGetFrame(PDBG_STREAM psStream)
{
	IMG_UINT32	ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret=DBGDrivGetFrame(psStream);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}

/*!
 @name	ExtDBGDrivIsLastCaptureFrame
 */
IMG_BOOL IMG_CALLCONV ExtDBGDrivIsLastCaptureFrame(PDBG_STREAM psStream)
{
	IMG_BOOL	bRet;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	bRet = DBGDrivIsLastCaptureFrame(psStream);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return bRet;
}

/*!
 @name	ExtDBGDrivIsCaptureFrame
 */
IMG_BOOL IMG_CALLCONV ExtDBGDrivIsCaptureFrame(PDBG_STREAM psStream, IMG_BOOL bCheckPreviousFrame)
{
	IMG_BOOL	bRet;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	bRet = DBGDrivIsCaptureFrame(psStream, bCheckPreviousFrame);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return bRet;
}

/*!
 @name	ExtDBGDrivOverrideMode
 */
void IMG_CALLCONV ExtDBGDrivOverrideMode(PDBG_STREAM psStream,IMG_UINT32 ui32Mode)
{
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	DBGDrivOverrideMode(psStream, ui32Mode);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return;
}

/*!
 @name	ExtDBGDrivDefaultMode
 */
void IMG_CALLCONV ExtDBGDrivDefaultMode(PDBG_STREAM psStream)
{
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	DBGDrivDefaultMode(psStream);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return;
}

/*!
 @name	ExtDBGDrivWrite2
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivWrite2(PDBG_STREAM psStream,IMG_UINT8 * pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level)
{
	IMG_UINT32	ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret=DBGDrivWrite2(psStream, pui8InBuf, ui32InBuffSize, ui32Level);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}

/*!
 @name	ExtDBGDrivWritePersist
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivWritePersist(PDBG_STREAM psStream,IMG_UINT8 *pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level)
{
	IMG_UINT32	ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret=DBGDrivWritePersist(psStream, pui8InBuf, ui32InBuffSize, ui32Level);
	if(ui32Ret==0xFFFFFFFFU)
	{
		PVR_DPF((PVR_DBG_ERROR, "An error occurred in DBGDrivWritePersist."));
	}

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}

/*!
 @name	ExtDBGDrivWriteStringCM
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivWriteStringCM(PDBG_STREAM psStream,IMG_CHAR * pszString,IMG_UINT32 ui32Level)
{
	IMG_UINT32	ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret=DBGDrivWriteStringCM(psStream, pszString, ui32Level);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}

/*!
 @name	ExtDBGDrivWriteCM
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivWriteCM(PDBG_STREAM psStream,IMG_UINT8 * pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level)
{
	IMG_UINT32	ui32Ret;
	
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);
	
	ui32Ret=DBGDrivWriteCM(psStream, pui8InBuf, ui32InBuffSize, ui32Level);
	
	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);
	
	return ui32Ret;
}

/*!
 @name	ExtDBGDrivSetMarker
 */
void IMG_CALLCONV ExtDBGDrivSetMarker(PDBG_STREAM psStream, IMG_UINT32 ui32Marker)
{
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	DBGDrivSetMarker(psStream, ui32Marker);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return;
}

/*!
 @name	ExtDBGDrivGetMarker
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivGetMarker(PDBG_STREAM psStream)
{
	IMG_UINT32	ui32Marker;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Marker = DBGDrivGetMarker(psStream);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Marker;
}

/*!
 @name	ExtDBGDrivWriteLF
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivWriteLF(PDBG_STREAM psStream, IMG_UINT8 * pui8InBuf, IMG_UINT32 ui32InBuffSize, IMG_UINT32 ui32Level, IMG_UINT32 ui32Flags)
{
	IMG_UINT32	ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret = DBGDrivWriteLF(psStream, pui8InBuf, ui32InBuffSize, ui32Level, ui32Flags);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}

/*!
 @name	ExtDBGDrivReadLF
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivReadLF(PDBG_STREAM psStream, IMG_UINT32 ui32OutBuffSize, IMG_UINT8 * pui8OutBuf)
{
	IMG_UINT32 ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret = DBGDrivReadLF(psStream, ui32OutBuffSize, pui8OutBuf);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}


/*!
 @name	ExtDBGDrivStartInitPhase
 */
IMG_VOID IMG_CALLCONV ExtDBGDrivStartInitPhase(PDBG_STREAM psStream)
{
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	DBGDrivStartInitPhase(psStream);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return;
}

/*!
 @name	ExtDBGDrivStopInitPhase
 */
IMG_VOID IMG_CALLCONV ExtDBGDrivStopInitPhase(PDBG_STREAM psStream, IMG_BOOL bTakeMutex)
{
	/* Aquire API Mutex, not for WDDM which has a workaround for multi-app support */
	if (bTakeMutex) HostAquireMutex(g_pvAPIMutex);

	DBGDrivStopInitPhase(psStream);

	/* Release API Mutex */
	if (bTakeMutex) HostReleaseMutex(g_pvAPIMutex);

	return;
}

/*!
 @name	ExtDBGDrivIsInitPhase
 */
IMG_BOOL IMG_CALLCONV ExtDBGDrivIsInitPhase(PDBG_STREAM psStream)
{
	IMG_BOOL bInitPhase;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	bInitPhase = DBGDrivIsInitPhase(psStream);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return bInitPhase;
}

/*!
 @name	ExtDBGDrivGetStreamOffset
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivGetStreamOffset(PDBG_STREAM psStream)
{
	IMG_UINT32 ui32Ret;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32Ret = DBGDrivGetStreamOffset(psStream);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32Ret;
}

/*!
 @name	ExtDBGDrivSetStreamOffset
 */
IMG_VOID IMG_CALLCONV ExtDBGDrivSetStreamOffset(PDBG_STREAM psStream, IMG_UINT32 ui32StreamOffset)
{
	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	DBGDrivSetStreamOffset(psStream, ui32StreamOffset);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);
}

/*!
 @name	ExtDBGDrivWaitForEvent
 */
IMG_VOID IMG_CALLCONV ExtDBGDrivWaitForEvent(DBG_EVENT eEvent)
{
#if defined(SUPPORT_DBGDRV_EVENT_OBJECTS)
	DBGDrivWaitForEvent(eEvent);
#else	/* defined(SUPPORT_DBGDRV_EVENT_OBJECTS) */
	PVR_UNREFERENCED_PARAMETER(eEvent);				/* PRQA S 3358 */
#endif	/* defined(SUPPORT_DBGDRV_EVENT_OBJECTS) */
}


/*!
 @name	ExtDBGDrivGetCtrlState
 */
IMG_UINT32 IMG_CALLCONV ExtDBGDrivGetCtrlState(PDBG_STREAM psStream, IMG_UINT32 ui32StateID)
{
	IMG_UINT32 ui32State = 0;

	/* Aquire API Mutex */
	HostAquireMutex(g_pvAPIMutex);

	ui32State = DBGDrivGetCtrlState(psStream, ui32StateID);

	/* Release API Mutex */
	HostReleaseMutex(g_pvAPIMutex);

	return ui32State;
}



/*!****************************************************************************
 @name		AtoI
 @brief		Returns the integer value of a decimal string
 @param		szIn - String with hexadecimal value
 @return	IMG_UINT32 integer value, 0 if string is null or not valid
				Based on Max`s one, now copes with (only) hex ui32ords, upper or lower case a-f.
*****************************************************************************/
IMG_UINT32 AtoI(IMG_CHAR *szIn)
{
	IMG_INT		iLen = 0;
	IMG_UINT32	ui32Value = 0;
	IMG_UINT32	ui32Digit=1;
	IMG_UINT32	ui32Base=10;
	IMG_INT		iPos;
	IMG_CHAR	bc;

	//get len of string
	while (szIn[iLen] > 0)
	{
		iLen ++;
	}

	//nothing to do
	if (iLen == 0)
	{
		return (0);
	}

	/* See if we have an 'x' or 'X' before the number to make it a hex number */
	iPos=0;
	while (szIn[iPos] == '0')
	{
		iPos++;
	}
	if (szIn[iPos] == '\0')
	{
		return 0;
	}
	if (szIn[iPos] == 'x' || szIn[iPos] == 'X')
	{
		ui32Base=16;
		szIn[iPos]='0';
	}

	//go through string from right (least significant) to left
	for (iPos = iLen - 1; iPos >= 0; iPos --)
	{
		bc = szIn[iPos];

		if ( (bc >= 'a') && (bc <= 'f') && ui32Base == 16)			//handle lower case a-f
		{
			bc -= 'a' - 0xa;
		}
		else
		if ( (bc >= 'A') && (bc <= 'F') && ui32Base == 16)			//handle upper case A-F
		{
			bc -= 'A' - 0xa;
		}
		else
		if ((bc >= '0') && (bc <= '9'))				//if char out of range, return 0
		{
			bc -= '0';
		}
		else
			return (0);

		ui32Value += (IMG_UINT32)bc  * ui32Digit;

		ui32Digit = ui32Digit * ui32Base;
	}
	return (ui32Value);
}


/*!****************************************************************************
 @name		StreamValid
 @brief		Validates supplied debug buffer.
 @param		psStream - debug stream
 @return	true if valid
*****************************************************************************/
static IMG_BOOL StreamValid(PDBG_STREAM psStream)
{
	PDBG_STREAM	psThis;

	psThis = g_psStreamList;

	while (psThis)
	{
		if (psStream && (psThis == psStream) )
		{
			return(IMG_TRUE);
		}
		else
		{
			psThis = psThis->psNext;
		}
	}

	return(IMG_FALSE);
}


/*!****************************************************************************
 @name		StreamValidForRead
 @brief		Validates supplied debug buffer for read op.
 @param		psStream - debug stream
 @return	true if readable
*****************************************************************************/
static IMG_BOOL StreamValidForRead(PDBG_STREAM psStream)
{
	if( StreamValid(psStream) &&
		((psStream->psCtrl->ui32Flags & DEBUG_FLAGS_WRITEONLY) == 0) )
	{
		return(IMG_TRUE);
	}

	return(IMG_FALSE);
}

/*!****************************************************************************
 @name		StreamValidForWrite
 @brief		Validates supplied debug buffer for write op.
 @param		psStream - debug stream
 @return	true if writable
*****************************************************************************/
static IMG_BOOL StreamValidForWrite(PDBG_STREAM psStream)
{
	if( StreamValid(psStream) &&
		((psStream->psCtrl->ui32Flags & DEBUG_FLAGS_READONLY) == 0) )
	{
		return(IMG_TRUE);
	}

	return(IMG_FALSE);
}


/*!****************************************************************************
 @name		Write
 @brief		Copies data from a buffer into selected stream. Stream size is fixed.
 @param		psStream - stream for output
 @param		pui8Data - input buffer
 @param		ui32InBuffSize - size of input
 @return	none
*****************************************************************************/
static void Write(PDBG_STREAM psStream,IMG_PUINT8 pui8Data,IMG_UINT32 ui32InBuffSize)
{
	/*
		Split copy into two bits as necessary (if we're allowed to wrap).
	*/
	if (!psStream->bCircularAllowed)
	{
		//PVR_ASSERT( (psStream->ui32WPtr + ui32InBuffSize) < psStream->ui32Size );
	}

	if ((psStream->ui32WPtr + ui32InBuffSize) > psStream->ui32Size)
	{
		/* Yes we need two bits, calculate their sizes */
		IMG_UINT32 ui32B1 = psStream->ui32Size - psStream->ui32WPtr;
		IMG_UINT32 ui32B2 = ui32InBuffSize - ui32B1;

		/* Copy first block to current location */
		HostMemCopy((IMG_PVOID)((IMG_UINTPTR_T)psStream->pvBase + psStream->ui32WPtr),
				(IMG_PVOID) pui8Data,
				ui32B1);

		/* Copy second block to start of buffer */
		HostMemCopy(psStream->pvBase,
				(IMG_PVOID)(pui8Data + ui32B1),
				ui32B2);

		/* Set pointer to be the new end point */
		psStream->ui32WPtr = ui32B2;
	}
	else
	{	/* Can fit block in single chunk */
		HostMemCopy((IMG_PVOID)((IMG_UINTPTR_T)psStream->pvBase + psStream->ui32WPtr),
				(IMG_PVOID) pui8Data,
				ui32InBuffSize);

		psStream->ui32WPtr += ui32InBuffSize;

		if (psStream->ui32WPtr == psStream->ui32Size)
		{
			psStream->ui32WPtr = 0;
		}
	}
	psStream->ui32DataWritten += ui32InBuffSize;
}


/*!****************************************************************************
 @name		MonoOut
 @brief		Output data to mono display. [Possibly deprecated]
 @param		pszString - input
 @param		bNewLine - line wrapping
 @return	none
*****************************************************************************/
void MonoOut(IMG_CHAR * pszString,IMG_BOOL bNewLine)
{
#if defined (_WIN64)
	PVR_UNREFERENCED_PARAMETER(pszString);
	PVR_UNREFERENCED_PARAMETER(bNewLine);

#else
	IMG_UINT32 	i;
	IMG_CHAR *	pScreen;

	pScreen = (IMG_CHAR *) DBGDRIV_MONOBASE;

	pScreen += g_ui32Line * 160;

	/*
		Write the string.
	*/
	i=0;
	do
	{
		pScreen[g_ui32LOff + (i*2)] = pszString[i];
		pScreen[g_ui32LOff + (i*2)+1] = 127;
		i++;
	}
	while ((pszString[i] != 0) && (i < 4096));

	g_ui32LOff += i * 2;

	if (bNewLine)
	{
		g_ui32LOff = 0;
		g_ui32Line++;
	}

	/*
		Scroll if necssary.
	*/
	if (g_ui32Line == g_ui32MonoLines)
	{
		g_ui32Line = g_ui32MonoLines - 1;

		HostMemCopy((IMG_VOID *)(IMG_UINTPTR_T)DBGDRIV_MONOBASE,(IMG_VOID *)(IMG_UINTPTR_T)(DBGDRIV_MONOBASE + 160),160 * (g_ui32MonoLines - 1));

		HostMemSet((IMG_VOID *)(IMG_UINTPTR_T)(DBGDRIV_MONOBASE + (160 * (g_ui32MonoLines - 1))),0,160);
	}
#endif	
}

/*!****************************************************************************
 @name		WriteExpandingBuffer
 @brief		Copies data from a buffer into selected stream. Stream size may be expandable.
 @param		psStream - stream for output
 @param		pui8InBuf - input buffer
 @param		ui32InBuffSize - size of input
 @return	bytes copied
*****************************************************************************/
static IMG_UINT32 WriteExpandingBuffer(PDBG_STREAM psStream,IMG_UINT8 * pui8InBuf,IMG_UINT32 ui32InBuffSize)
{
	IMG_UINT ui32Space;

	/*
		How much space have we got in the buffer ?
	*/
	ui32Space = SpaceInStream(psStream);

	/*
		Don't copy anything if we don't have space or buffers not enabled.
	*/
	if ((psStream->psCtrl->ui32OutMode & DEBUG_OUTMODE_STREAMENABLE) == 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "WriteExpandingBuffer: buffer %p is disabled", psStream));
		return(0);
	}

	/*
		Check if we can expand the buffer 
	*/
	if (psStream->psCtrl->ui32Flags & DEBUG_FLAGS_NO_BUF_EXPANDSION)
	{
		/*
			Don't do anything if we've got less that 32 ui8tes of space and
			we're not allowing expansion of buffer space...
		*/
		if (ui32Space < 32)
		{
			PVR_DPF((PVR_DBG_ERROR, "WriteExpandingBuffer: buffer %p is full and isn't expandable", psStream));
			return(0);
		}
	}
	else
	{
		if ((ui32Space < 32) || (ui32Space <= (ui32InBuffSize + 4)))
		{
			IMG_UINT32	ui32NewBufSize;

			/*
				Find new buffer size, double the current size or increase by 1MB
			*/
			ui32NewBufSize = MIN(psStream->ui32Size<<1,psStream->ui32Size+(1<<20));

			PVR_DPF((PVR_DBGDRIV_MESSAGE, "Expanding buffer size = %x, new size = %x",
					psStream->ui32Size, ui32NewBufSize));

			if (ui32InBuffSize > psStream->ui32Size)
			{
				ui32NewBufSize += ui32InBuffSize;
			}

			/* 
				Attempt to expand the buffer 
			*/
			if (!ExpandStreamBuffer(psStream,ui32NewBufSize))
			{
				if (ui32Space < 32)
				{
					if(psStream->bCircularAllowed)
					{
						return(0);
					}
					else
					{
						/* out of memory */
						PVR_DPF((PVR_DBG_ERROR, "WriteExpandingBuffer: Unable to expand %p. Out of memory.", psStream));
						InvalidateAllStreams();
						return (0xFFFFFFFFUL);
					}
				}
			}

			/* 
				Recalc the space in the buffer 
			*/
			ui32Space = SpaceInStream(psStream);
			PVR_DPF((PVR_DBGDRIV_MESSAGE, "Expanded buffer, free space = %x",
					ui32Space));
		}
	}

	/*
		Only copy what we can..
	*/
	if (ui32Space <= (ui32InBuffSize + 4))
	{
		ui32InBuffSize = ui32Space - 4;
	}

	/*
		Write the stuff...
	*/
	Write(psStream,pui8InBuf,ui32InBuffSize);

#if defined(SUPPORT_DBGDRV_EVENT_OBJECTS)
	if (ui32InBuffSize)
	{
		HostSignalEvent(DBG_EVENT_STREAM_DATA);
	}
#endif
	return(ui32InBuffSize);
}

/*****************************************************************************
******************************************************************************
******************************************************************************
 THE ACTUAL FUNCTIONS
******************************************************************************
******************************************************************************
*****************************************************************************/

/*!****************************************************************************
 @name		DBGDrivCreateStream
 @brief		Creates a pdump/debug stream
 @param		pszName - stream name
 @param		ui32CapMode - capture mode (framed, continuous, hotkey)
 @param		ui32OutMode - output mode (see dbgdrvif.h)
 @param		ui32Flags - output flags, text stream bit is set for pdumping
 @param		ui32Size - size of stream buffer in pages
 @return	none
*****************************************************************************/
IMG_VOID * IMG_CALLCONV DBGDrivCreateStream(IMG_CHAR *		pszName,
								   IMG_UINT32 	ui32CapMode,
								   IMG_UINT32 	ui32OutMode,
								   IMG_UINT32	ui32Flags,
								   IMG_UINT32 	ui32Size)
{
	PDBG_STREAM psStream;
	PDBG_STREAM	psInitStream;
	PDBG_LASTFRAME_BUFFER	psLFBuffer;
	PDBG_LASTFRAME_BUFFER	psLFDeinitBuffer;
	PDBG_STREAM_CONTROL psCtrl;
	IMG_UINT32		ui32Off;
	IMG_VOID *		pvBase;	
	static IMG_CHAR pszNameInitSuffix[] = "_Init";
	IMG_UINT32		ui32OffSuffix;

	/*
		If we already have a buffer using this name just return
		its handle.
	*/
	psStream = (PDBG_STREAM) DBGDrivFindStream(pszName, IMG_FALSE);

	if (psStream)
	{
		return ((IMG_VOID *) psStream);
	}

	/*
		Allocate memory for control structures
	*/
	psStream = HostNonPageablePageAlloc(1);
	psInitStream = HostNonPageablePageAlloc(1);
	psLFBuffer = HostNonPageablePageAlloc(1);
	psLFDeinitBuffer = HostNonPageablePageAlloc(1);
	psCtrl = HostNonPageablePageAlloc(1);
	if	(
			(!psStream) ||
			(!psInitStream) ||
			(!psLFBuffer) ||
			(!psLFDeinitBuffer) ||
			(!psCtrl)
		)
	{
		PVR_DPF((PVR_DBG_ERROR,"DBGDriv: Couldn't alloc control structs\n\r"));
		return((IMG_VOID *) 0);
	}

	/* Allocate memory for buffer */
	if ((ui32Flags & DEBUG_FLAGS_USE_NONPAGED_MEM) != 0)
	{
		pvBase = HostNonPageablePageAlloc(ui32Size);
	}
	else
	{
		pvBase = HostPageablePageAlloc(ui32Size);
	}

	if (!pvBase)
	{
		PVR_DPF((PVR_DBG_ERROR,"DBGDriv: Couldn't alloc Stream buffer\n\r"));
		HostNonPageablePageFree(psStream);
		return((IMG_VOID *) 0);
	}

	/* Setup control state */
	psCtrl->ui32Flags = ui32Flags;
	psCtrl->ui32CapMode = ui32CapMode;
	psCtrl->ui32OutMode = ui32OutMode;
	psCtrl->ui32DebugLevel = DEBUG_LEVEL_0;
	psCtrl->ui32DefaultMode = ui32CapMode;
	psCtrl->ui32Start = 0;
	psCtrl->ui32End = 0;
	psCtrl->ui32Current = 0;
	psCtrl->ui32SampleRate = 1;
	psCtrl->bInitPhaseComplete = IMG_FALSE;

	/*
		Setup internal debug buffer state.
	*/
	psStream->psNext = 0;
	psStream->pvBase = pvBase;
	psStream->psCtrl = psCtrl;
	psStream->ui32Size = ui32Size * 4096UL;
	psStream->ui32RPtr = 0;
	psStream->ui32WPtr = 0;
	psStream->ui32DataWritten = 0;
	psStream->ui32Marker = 0;
	psStream->bCircularAllowed = IMG_TRUE;
	psStream->ui32InitPhaseWOff = 0;

	/* Allocate memory for buffer */
	if ((ui32Flags & DEBUG_FLAGS_USE_NONPAGED_MEM) != 0)
	{
		pvBase = HostNonPageablePageAlloc(ui32Size);
	}
	else
	{
		pvBase = HostPageablePageAlloc(ui32Size);
	}

	if (!pvBase)
	{
		PVR_DPF((PVR_DBG_ERROR,"DBGDriv: Couldn't alloc InitStream buffer\n\r"));
		
		if ((psStream->psCtrl->ui32Flags & DEBUG_FLAGS_USE_NONPAGED_MEM) != 0)
		{
			HostNonPageablePageFree(psStream->pvBase);
		}
		else
		{
			HostPageablePageFree(psStream->pvBase);
		}
		HostNonPageablePageFree(psStream);
		return((IMG_VOID *) 0);
	}

	/* Initialise the stream for the init phase */
	psInitStream->psNext = 0;
	psInitStream->pvBase = pvBase;
	psInitStream->psCtrl = psCtrl;
	psInitStream->ui32Size = ui32Size * 4096UL;
	psInitStream->ui32RPtr = 0;
	psInitStream->ui32WPtr = 0;
	psInitStream->ui32DataWritten = 0;
	psInitStream->ui32Marker = 0;
	psInitStream->bCircularAllowed = IMG_FALSE;
	psInitStream->ui32InitPhaseWOff = 0;
	psStream->psInitStream = psInitStream;

	/* Setup last frame buffer */
	psLFBuffer->psStream = psStream;
	psLFBuffer->ui32BufLen = 0UL;

	/* Setup last frame deinit buffer */
	psLFDeinitBuffer->psStream = psStream;
	psLFDeinitBuffer->ui32BufLen = 0UL;

	g_bHotkeyMiddump = IMG_FALSE;
	g_ui32HotkeyMiddumpStart = 0xffffffffUL;
	g_ui32HotkeyMiddumpEnd = 0xffffffffUL;

	/*
		Copy buffer name.
	*/
	ui32Off = 0;

	do
	{
		psStream->szName[ui32Off] = pszName[ui32Off];
		psInitStream->szName[ui32Off] = pszName[ui32Off];
		ui32Off++;
	}
	while ((pszName[ui32Off] != 0) && (ui32Off < (4096UL - sizeof(DBG_STREAM))));
	psStream->szName[ui32Off] = pszName[ui32Off];	/* PRQA S 3689 */

	/*
		Append suffix to init phase name
	*/
	ui32OffSuffix = 0;
	do
	{
		psInitStream->szName[ui32Off] = pszNameInitSuffix[ui32OffSuffix];
		ui32Off++;
		ui32OffSuffix++;
	}
	while (	(pszNameInitSuffix[ui32OffSuffix] != 0) &&
			(ui32Off < (4096UL - sizeof(DBG_STREAM))));
	psInitStream->szName[ui32Off] = pszNameInitSuffix[ui32OffSuffix];	/* PRQA S 3689 */

	/*
		Insert into list.
	*/
	psStream->psNext = g_psStreamList;
	g_psStreamList = psStream;

	psLFBuffer->psNext = g_psLFBufferList;
	g_psLFBufferList = psLFBuffer;

	psLFDeinitBuffer->psNext = g_psLFDeinitBufferList;
	g_psLFDeinitBufferList = psLFDeinitBuffer;

	AddSIDEntry(psStream);
	
	return((IMG_VOID *) psStream);
}

/*!****************************************************************************
 @name		DBGDrivDestroyStream
 @brief		Delete a stream and free its memory
 @param		psStream - stream to be removed
 @return	none
*****************************************************************************/
void IMG_CALLCONV DBGDrivDestroyStream(PDBG_STREAM psStream)
{
	PDBG_STREAM	psStreamThis;
	PDBG_STREAM	psStreamPrev;
	PDBG_LASTFRAME_BUFFER	psLFBuffer;
	PDBG_LASTFRAME_BUFFER	psLFDeinitBuffer;
	PDBG_LASTFRAME_BUFFER	psLFThis;
	PDBG_LASTFRAME_BUFFER	psLFPrev;

	PVR_DPF((PVR_DBG_MESSAGE, "DBGDriv: Destroying stream %s\r\n", psStream->szName ));

	/*
		Validate buffer.
	*/
	if (!StreamValid(psStream))
	{
		return;
	}

	RemoveSIDEntry(psStream);
	
	/*
		Remove from linked list.
	*/
	psStreamThis = g_psStreamList;
	psStreamPrev = 0;

	while (psStreamThis)
	{
		if (psStreamThis == psStream)
		{
			if (psStreamPrev)
			{
				psStreamPrev->psNext = psStreamThis->psNext;
			}
			else
			{
				g_psStreamList = psStreamThis->psNext;
			}

			psStreamThis = 0;
		}
		else
		{
			psStreamPrev = psStreamThis;
			psStreamThis = psStreamThis->psNext;
		}
	}

	/*
		Remove LF from linked list.
	*/
	psLFBuffer = FindLFBufFromList(psStream, g_psLFBufferList);
	psLFThis = g_psLFBufferList;
	psLFPrev = 0;

	while (psLFThis)
	{
		if (psLFThis == psLFBuffer)
		{
			if (psLFPrev)
			{
				psLFPrev->psNext = psLFThis->psNext;
			}
			else
			{
				g_psLFBufferList = psLFThis->psNext;
			}

			psLFThis = 0;
		}
		else
		{
			psLFPrev = psLFThis;
			psLFThis = psLFThis->psNext;
		}
	}

	/*
		Remove LF deinit from linked list.
	*/
	psLFDeinitBuffer = FindLFBufFromList(psStream, g_psLFDeinitBufferList);
	psLFThis = g_psLFDeinitBufferList;
	psLFPrev = 0;

	while (psLFThis)
	{
		if (psLFThis == psLFDeinitBuffer)
		{
			if (psLFPrev)
			{
				psLFPrev->psNext = psLFThis->psNext;
			}
			else
			{
				g_psLFDeinitBufferList = psLFThis->psNext;
			}

			psLFThis = 0;
		}
		else
		{
			psLFPrev = psLFThis;
			psLFThis = psLFThis->psNext;
		}
	}

	/*
		Dectivate hotkey it the stream is of this type.
	*/
	if (psStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_HOTKEY)
	{
		DeactivateHotKeys();
	}

	/*
		And free its memory.
	*/
	if ((psStream->psCtrl->ui32Flags & DEBUG_FLAGS_USE_NONPAGED_MEM) != 0)
	{
		HostNonPageablePageFree(psStream->psCtrl);
		HostNonPageablePageFree(psStream->pvBase);
		HostNonPageablePageFree(psStream->psInitStream->pvBase);
	}
	else
	{
		HostNonPageablePageFree(psStream->psCtrl);
		HostPageablePageFree(psStream->pvBase);
		HostPageablePageFree(psStream->psInitStream->pvBase);
	}

	HostNonPageablePageFree(psStream->psInitStream);
	HostNonPageablePageFree(psStream);
	HostNonPageablePageFree(psLFBuffer);
	HostNonPageablePageFree(psLFDeinitBuffer);

	if (g_psStreamList == 0)
	{
		PVR_DPF((PVR_DBG_MESSAGE,"DBGDriv: Stream list now empty" ));
	}

	return;
}

/*!****************************************************************************
 @name		DBGDrivFindStream
 @brief		Finds/resets a named stream
 @param		pszName - stream name
 @param		bResetStream - whether to reset the stream, e.g. to end pdump init phase
 @return	none
*****************************************************************************/
IMG_VOID * IMG_CALLCONV DBGDrivFindStream(IMG_CHAR * pszName, IMG_BOOL bResetStream)
{
	PDBG_STREAM	psStream;
	PDBG_STREAM	psThis;
	IMG_UINT32	ui32Off;
	IMG_BOOL	bAreSame;

	psStream = 0;

	PVR_DPF((PVR_DBGDRIV_MESSAGE, "PDump client connecting to %s %s",
			pszName,
			(bResetStream == IMG_TRUE) ? "with reset" : "no reset"));

	/*
		Scan buffer names for supplied one.
	*/
	for (psThis = g_psStreamList; psThis != IMG_NULL; psThis = psThis->psNext)
	{
		bAreSame = IMG_TRUE;
		ui32Off = 0;

		if (strlen(psThis->szName) == strlen(pszName))
		{
			while ((psThis->szName[ui32Off] != 0) && (pszName[ui32Off] != 0) && (ui32Off < 128) && bAreSame)
			{
				if (psThis->szName[ui32Off] != pszName[ui32Off])
				{
					bAreSame = IMG_FALSE;
				}

				ui32Off++;
			}
		}
		else
		{
			bAreSame = IMG_FALSE;
		}

		if (bAreSame)
		{
			psStream = psThis;
			break;
		}
	}

	if(bResetStream && psStream)
	{
		static IMG_CHAR szComment[] = "-- Init phase terminated\r\n";
		psStream->psInitStream->ui32RPtr = 0;
		psStream->ui32RPtr = 0;
		psStream->ui32WPtr = 0;
		psStream->ui32DataWritten = psStream->psInitStream->ui32DataWritten;
		if (psStream->psCtrl->bInitPhaseComplete == IMG_FALSE)
		{
			if (psStream->psCtrl->ui32Flags & DEBUG_FLAGS_TEXTSTREAM)
			{
				DBGDrivWrite2(psStream, (IMG_UINT8 *)szComment, sizeof(szComment) - 1, 0x01);
			}
			psStream->psCtrl->bInitPhaseComplete = IMG_TRUE;
		}

		{
			/* mark init stream to prevent further reading by pdump client */
			/* Check for possible race condition */
			psStream->psInitStream->ui32InitPhaseWOff = psStream->psInitStream->ui32WPtr;
			PVR_DPF((PVR_DBGDRIV_MESSAGE, "Set %s client marker bo %x, total bw %x",
					psStream->szName,
					psStream->psInitStream->ui32InitPhaseWOff,
					psStream->psInitStream->ui32DataWritten ));
		}
	}

	return((IMG_VOID *) psStream);
}

static void IMG_CALLCONV DBGDrivInvalidateStream(PDBG_STREAM psStream)
{
	IMG_CHAR pszErrorMsg[] = "**OUTOFMEM\n";
	IMG_UINT32 ui32Space;
	IMG_UINT32 ui32Off = 0;
	IMG_UINT32 ui32WPtr = psStream->ui32WPtr;
	IMG_PUINT8 pui8Buffer = (IMG_UINT8 *) psStream->pvBase;
	
	PVR_DPF((PVR_DBG_ERROR, "DBGDrivInvalidateStream: An error occurred for stream %s\r\n", psStream->szName ));

	/*
		Validate buffer.
	*/
	/*
	if (!StreamValid(psStream))
	{
		return;
	}
*/
	/* Write what we can of the error message */
	ui32Space = SpaceInStream(psStream);

	/* Make sure there's space for termination character */
	if(ui32Space > 0)
	{
		ui32Space--;
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "DBGDrivInvalidateStream: Buffer full."));
	}

	while((pszErrorMsg[ui32Off] != 0) && (ui32Off < ui32Space))
	{
		pui8Buffer[ui32WPtr] = (IMG_UINT8)pszErrorMsg[ui32Off];
		ui32Off++;
		ui32WPtr++;
	}
	pui8Buffer[ui32WPtr++] = '\0';
	psStream->ui32WPtr = ui32WPtr;

	/* Buffer will accept no more params from Services/client driver */
	psStream->psCtrl->ui32Flags |= DEBUG_FLAGS_READONLY;
}

/*!****************************************************************************
 @name		InvalidateAllStreams
 @brief		invalidate all streams in list
 @return	none
*****************************************************************************/
static IMG_VOID InvalidateAllStreams(IMG_VOID)
{
	PDBG_STREAM psStream = g_psStreamList;
	while (psStream != IMG_NULL)
	{
		DBGDrivInvalidateStream(psStream);
		psStream = psStream->psNext;
	}
	return;
}



/*!****************************************************************************
 @name		DBGDrivWriteStringCM
 @brief		Write capture mode data, wraps DBGDrivWriteString
 @param		psStream - stream
 @param		pszString - input buffer
 @param		ui32Level - debug level
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivWriteStringCM(PDBG_STREAM psStream,IMG_CHAR * pszString,IMG_UINT32 ui32Level)
{
	/*
		Validate buffer.
	*/
	if (!StreamValidForWrite(psStream))
	{
		return(0xFFFFFFFFUL);
	}

	/*
		Only write string if debug capture mode adds up...
	*/
	if (!psStream->psCtrl->bInitPhaseComplete)
	{
		/* Always capture during the init phase */
	}
	else if (psStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_FRAMED)
	{
		if	((psStream->psCtrl->ui32Flags & DEBUG_FLAGS_ENABLESAMPLE) == 0)
		{
			return(0);
		}
	}
	else
	{
		if (psStream->psCtrl->ui32CapMode == DEBUG_CAPMODE_HOTKEY)
		{
			if ((psStream->psCtrl->ui32Current != g_ui32HotKeyFrame) || (g_bHotKeyPressed == IMG_FALSE))
			{
				return(0);
			}
		}
	}

	return(DBGDrivWriteString(psStream,pszString,ui32Level));

}

/*!****************************************************************************
 @name		DBGDrivWriteString
 @brief		Write string to stream (note stream buffer size is assumed fixed)
 @param		psStream - stream
 @param		pszString - string to write
 @param		ui32Level - verbosity level
 @return	-1; invalid stream
 			0;	other error (e.g. stream not enabled)
 			else number of characters written
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivWriteString(PDBG_STREAM psStream,IMG_CHAR * pszString,IMG_UINT32 ui32Level)
{
	IMG_UINT32	ui32Len;
	IMG_UINT32	ui32Space;
	IMG_UINT32	ui32WPtr;
	IMG_UINT8 *	pui8Buffer;

	/*
		Validate buffer.
	*/
	if (!StreamValidForWrite(psStream))
	{
		return(0xFFFFFFFFUL);
	}

	/*
		Check debug level.
	*/
	if ((psStream->psCtrl->ui32DebugLevel & ui32Level) == 0)
	{
		return(0xFFFFFFFFUL);
	}

	/*
		Output to standard debug out ? (don't if async out
		flag is set).
	*/
	if ((psStream->psCtrl->ui32OutMode & DEBUG_OUTMODE_ASYNC) == 0)
	{
		if (psStream->psCtrl->ui32OutMode & DEBUG_OUTMODE_STANDARDDBG)
		{
			PVR_DPF((PVR_DBG_MESSAGE,"%s: %s\r\n",psStream->szName, pszString));
		}

		/*
			Output to mono monitor ?
		*/
		if (psStream->psCtrl->ui32OutMode & DEBUG_OUTMODE_MONO)
		{
			MonoOut(psStream->szName,IMG_FALSE);
			MonoOut(": ",IMG_FALSE);
			MonoOut(pszString,IMG_TRUE);
		}
	}

	/*
		Don't bother writing the string if it's not flagged
	*/
	if	(
			!(
				((psStream->psCtrl->ui32OutMode & DEBUG_OUTMODE_STREAMENABLE) != 0) ||
				((psStream->psCtrl->ui32OutMode & DEBUG_OUTMODE_ASYNC) != 0)
			)
		)
	{
		return(0xFFFFFFFFUL);
	}

	/*
		How much space have we got in the buffer ?
	*/
	ui32Space=SpaceInStream(psStream);

	/* Make sure there's space for termination character */
	if(ui32Space > 0)
	{
		ui32Space--;
	}

	ui32Len		= 0;
	ui32WPtr	= psStream->ui32WPtr;
	pui8Buffer	= (IMG_UINT8 *) psStream->pvBase;

	while((pszString[ui32Len] != 0) && (ui32Len < ui32Space))
	{
		pui8Buffer[ui32WPtr] = (IMG_UINT8)pszString[ui32Len];
		ui32Len++;
		ui32WPtr++;
		if (ui32WPtr == psStream->ui32Size)
		{
			ui32WPtr = 0;
		}
	}

	if (ui32Len < ui32Space)
	{
		/* copy terminator */
		pui8Buffer[ui32WPtr] = (IMG_UINT8)pszString[ui32Len];
		ui32Len++;
		ui32WPtr++;
		if (ui32WPtr == psStream->ui32Size)
		{
			ui32WPtr = 0;
		}

		/* Write pointer, and length */
		psStream->ui32WPtr = ui32WPtr;
		psStream->ui32DataWritten+= ui32Len;
	} else
	{
		ui32Len = 0;
	}

#if defined(SUPPORT_DBGDRV_EVENT_OBJECTS)
	if (ui32Len)
	{
		HostSignalEvent(DBG_EVENT_STREAM_DATA);
	}
#endif

	return(ui32Len);
}

/*!****************************************************************************
 @name		DBGDrivReadString
 @brief		Reads string from debug stream
 @param		psStream - stream
 @param		pszString - string to read
 @param		ui32Limit - max size to read
 @return	-1; invalid stream
 			0;	other error (e.g. stream not enabled)
 			else number of characters read
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivReadString(PDBG_STREAM psStream,IMG_CHAR * pszString,IMG_UINT32 ui32Limit)
{
	IMG_UINT32				ui32OutLen;
	IMG_UINT32				ui32Len;
	IMG_UINT32				ui32Offset;
	IMG_UINT8				*pui8Buff;

	/*
		Validate buffer.
	*/
	if (!StreamValidForRead(psStream))
	{
		return(0);
	}

	/*
		Stream appears to be in list so carry on.
	*/
	pui8Buff = (IMG_UINT8 *)psStream->pvBase;
	ui32Offset = psStream->ui32RPtr;

	if (psStream->ui32RPtr == psStream->ui32WPtr)
	{
		return(0);
	}

	/*
		Find length of string.
	*/
	ui32Len = 0;
	while((pui8Buff[ui32Offset] != 0) && (ui32Offset != psStream->ui32WPtr))
	{
		ui32Offset++;
		ui32Len++;

		/*
			Reset offset if buffer wrapped.
		*/
		if (ui32Offset == psStream->ui32Size)
		{
			ui32Offset = 0;
		}
	}

	ui32OutLen = ui32Len + 1;

	/*
		Only copy string if target has enough space.
	*/
	if (ui32Len > ui32Limit)
	{
		return(0);
	}

	/*
		Copy it.
	*/
	ui32Offset = psStream->ui32RPtr;
	ui32Len = 0;

	while ((pui8Buff[ui32Offset] != 0) && (ui32Len < ui32Limit))
	{
		pszString[ui32Len] = (IMG_CHAR)pui8Buff[ui32Offset];
		ui32Offset++;
		ui32Len++;

		/*
			If wrap as necessary
		*/
		if (ui32Offset == psStream->ui32Size)
		{
			ui32Offset = 0;
		}
	}

	pszString[ui32Len] = (IMG_CHAR)pui8Buff[ui32Offset];

	psStream->ui32RPtr = ui32Offset + 1;

	if (psStream->ui32RPtr == psStream->ui32Size)
	{
		psStream->ui32RPtr = 0;
	}

	return(ui32OutLen);
}

/*!****************************************************************************
 @name		DBGDrivWrite
 @brief		Write binary buffer to stream (fixed size)
 @param		psStream - stream
 @param		pui8InBuf - buffer to write
 @param		ui32InBuffSize - size
 @param		ui32Level - verbosity level
 @return	bytes written, 0 if recoverable error, -1 if unrecoverable error
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivWrite(PDBG_STREAM psMainStream,IMG_UINT8 * pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level)
{
	IMG_UINT32				ui32Space;
	DBG_STREAM *psStream;

	/*
		Validate buffer.
	*/
	if (!StreamValidForWrite(psMainStream))
	{
		return(0xFFFFFFFFUL);
	}

	/*
		Check debug level.
	*/
	if ((psMainStream->psCtrl->ui32DebugLevel & ui32Level) == 0)
	{
		return(0xFFFFFFFFUL);
	}

	/*
		Only write data if debug mode adds up...
	*/
	if (psMainStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_FRAMED)
	{
		if	((psMainStream->psCtrl->ui32Flags & DEBUG_FLAGS_ENABLESAMPLE) == 0)
		{
			/* throw away non-capturing data */
			return(ui32InBuffSize);
		}
	}
	else if (psMainStream->psCtrl->ui32CapMode == DEBUG_CAPMODE_HOTKEY)
	{
		if ((psMainStream->psCtrl->ui32Current != g_ui32HotKeyFrame) || (g_bHotKeyPressed == IMG_FALSE))
		{
			/* throw away non-capturing data */
			return(ui32InBuffSize);
		}
	}

	if(psMainStream->psCtrl->bInitPhaseComplete)
	{
		psStream = psMainStream;
	}
	else
	{
		psStream = psMainStream->psInitStream;
	}

	/*
		How much space have we got in the buffer ?
	*/
	ui32Space=SpaceInStream(psStream);

	PVR_DPF((PVR_DBGDRIV_MESSAGE, "Recv %d b for %s: Roff = %x, WOff = %x",
			ui32InBuffSize,
			psStream->szName,
			psStream->ui32RPtr,
			psStream->ui32WPtr));

	/*
		Don't copy anything if we don't have space or buffers not enabled.
	*/
	if ((psStream->psCtrl->ui32OutMode & DEBUG_OUTMODE_STREAMENABLE) == 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "DBGDrivWrite: buffer %p is disabled", psStream));
		return(0);
	}

	if (ui32Space < 8)
	{
		PVR_DPF((PVR_DBG_ERROR, "DBGDrivWrite: buffer %p is full", psStream));
		return(0);
	}

	/*
		Only copy what we can..
	*/
	if (ui32Space <= (ui32InBuffSize + 4))
	{
		ui32InBuffSize = ui32Space - 8;
	}

	/*
		Write the stuff...
	*/
	Write(psStream,(IMG_UINT8 *) &ui32InBuffSize,4);
	Write(psStream,pui8InBuf,ui32InBuffSize);

#if defined(SUPPORT_DBGDRV_EVENT_OBJECTS)
	if (ui32InBuffSize)
	{
		HostSignalEvent(DBG_EVENT_STREAM_DATA);
	}
#endif
	return(ui32InBuffSize);
}

/*!****************************************************************************
 @name		DBGDrivWriteCM
 @brief		Write capture mode data, wraps DBGDrivWrite
 @param		psStream - stream
 @param		pui8InBuf - input buffer
 @param		ui32InBuffSize - buffer size
 @param		ui32Level - verbosity level
 @return	bytes written, 0 if recoverable error, -1 if unrecoverable error
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivWriteCM(PDBG_STREAM psStream,IMG_UINT8 * pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level)
{
	/*
		Validate buffer.
	*/
	if (!StreamValidForWrite(psStream))
	{
		return(0xFFFFFFFFUL);
	}

	/*
		Only write data if debug mode adds up...
	*/
	if (!psStream->psCtrl->bInitPhaseComplete)
	{
		/* Always capture during the init phase */
	}
	else if (psStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_FRAMED)
	{
		if	((psStream->psCtrl->ui32Flags & DEBUG_FLAGS_ENABLESAMPLE) == 0)
		{
			/* throw away non-capturing data */
			return(ui32InBuffSize);
		}
	}
	else
	{
		if (psStream->psCtrl->ui32CapMode == DEBUG_CAPMODE_HOTKEY)
		{
			if ((psStream->psCtrl->ui32Current != g_ui32HotKeyFrame) || (g_bHotKeyPressed == IMG_FALSE))
			{
				/* throw away non-capturing data */
				return(ui32InBuffSize);
			}
		}
	}

	return(DBGDrivWrite2(psStream,pui8InBuf,ui32InBuffSize,ui32Level));
}


/*!****************************************************************************
 @name		DBGDrivWritePersist
 @brief		Copies data from a buffer into selected stream's init phase. Stream size should be expandable.
 @param		psStream - stream for output
 @param		pui8InBuf - input buffer
 @param		ui32InBuffSize - size of input
 @param		ui32Level - not used
 @return	bytes copied, 0 if recoverable error, -1 if unrecoverable error
*****************************************************************************/
static IMG_UINT32 DBGDrivWritePersist(PDBG_STREAM psMainStream,IMG_UINT8 * pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level)
{
	DBG_STREAM	*psStream;
	PVR_UNREFERENCED_PARAMETER(ui32Level);

	/*
		Validate buffer.
	*/
	if (!StreamValidForWrite(psMainStream))
	{
		return(0xFFFFFFFFUL);
	}

	/* Always append persistent data to init phase so it's available on
	 * subsequent app runs.
	*/
	psStream = psMainStream->psInitStream;
	if(psStream->bCircularAllowed == IMG_TRUE)
	{
		PVR_DPF((PVR_DBG_WARNING, "DBGDrivWritePersist: Init phase is a circular buffer, some data may be lost"));
	}

	PVR_DPF((PVR_DBGDRIV_MESSAGE, "Append %x b to %s: Roff = %x, WOff = %x [bw = %x]",
			ui32InBuffSize,
			psStream->szName,
			psStream->ui32RPtr,
			psStream->ui32WPtr,
			psStream->ui32DataWritten));

	return( WriteExpandingBuffer(psStream, pui8InBuf, ui32InBuffSize) );
}

/*!****************************************************************************
 @name		DBGDrivWrite2
 @brief		Copies data from a buffer into selected (expandable) stream.
 @param		psMainStream - stream for output
 @param		pui8InBuf - input buffer
 @param		ui32InBuffSize - size of input
 @param		ui32Level - debug level of input
 @return	bytes copied, 0 if recoverable error, -1 if unrecoverable error
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivWrite2(PDBG_STREAM psMainStream,IMG_UINT8 * pui8InBuf,IMG_UINT32 ui32InBuffSize,IMG_UINT32 ui32Level)
{
	DBG_STREAM	*psStream;

	/*
		Validate buffer.
	*/
	if (!StreamValidForWrite(psMainStream))
	{
		PVR_DPF((PVR_DBG_ERROR, "DBGDrivWrite2: stream not valid"));
		return(0xFFFFFFFFUL);
	}

	/*
		Check debug level.
	*/
	if ((psMainStream->psCtrl->ui32DebugLevel & ui32Level) == 0)
	{
		return(0);
	}

	if(psMainStream->psCtrl->bInitPhaseComplete)
	{
		psStream = psMainStream;
	}
	else
	{
		psStream = psMainStream->psInitStream;
	}

	PVR_DPF((PVR_DBGDRIV_MESSAGE, "Recv(exp) %d b for %s: Roff = %x, WOff = %x",
			ui32InBuffSize,
			psStream->szName,
			psStream->ui32RPtr,
			psStream->ui32WPtr));

	return( WriteExpandingBuffer(psStream, pui8InBuf, ui32InBuffSize) );
}

/*!****************************************************************************
 @name		DBGDrivRead
 @brief		Read from debug driver buffers
 @param		psMainStream - stream
 @param		bReadInitBuffer - whether to read from the init stream or the main stream
 @param		ui32OutBuffSize - available space in client buffer
 @param		pui8OutBuf - output buffer
 @return	bytes read, 0 if failure occurred
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivRead(PDBG_STREAM psMainStream, IMG_BOOL bReadInitBuffer, IMG_UINT32 ui32OutBuffSize,IMG_UINT8 * pui8OutBuf)
{
	IMG_UINT32 ui32Data;
	DBG_STREAM *psStream;

	/*
		Validate buffer.
	*/
	if (!StreamValidForRead(psMainStream))
	{
		PVR_DPF((PVR_DBG_ERROR, "DBGDrivRead: buffer %p is invalid", psMainStream));
		return(0);
	}

	if(bReadInitBuffer)
	{
		psStream = psMainStream->psInitStream;
	}
	else
	{
		psStream = psMainStream;
	}

	/* Don't read beyond the init phase marker point */
	if (psStream->ui32RPtr == psStream->ui32WPtr ||
		((psStream->ui32InitPhaseWOff > 0) &&
		 (psStream->ui32RPtr >= psStream->ui32InitPhaseWOff)) )
	{
		return(0);
	}

	/*
		Get amount of data in buffer.
	*/
	if (psStream->ui32RPtr <= psStream->ui32WPtr)
	{
		ui32Data = psStream->ui32WPtr - psStream->ui32RPtr;
	}
	else
	{
		ui32Data = psStream->ui32WPtr + (psStream->ui32Size - psStream->ui32RPtr);
	}

	/*
		Don't read beyond the init phase marker point
	*/
	if ((psStream->ui32InitPhaseWOff > 0) &&
		(psStream->ui32InitPhaseWOff < psStream->ui32WPtr))
	{
		ui32Data = psStream->ui32InitPhaseWOff - psStream->ui32RPtr;
	}

	/*
		Only transfer what target buffer can handle.
	*/
	if (ui32Data > ui32OutBuffSize)
	{
		ui32Data = ui32OutBuffSize;
	}

	PVR_DPF((PVR_DBGDRIV_MESSAGE, "Send %x b from %s: Roff = %x, WOff = %x",
			ui32Data,
			psStream->szName,
			psStream->ui32RPtr,
			psStream->ui32WPtr));

	/*
		Split copy into two bits as necessay.
	*/
	if ((psStream->ui32RPtr + ui32Data) > psStream->ui32Size)
	{	/* Calc block 1 and block 2 sizes */
		IMG_UINT32 ui32B1 = psStream->ui32Size - psStream->ui32RPtr;
		IMG_UINT32 ui32B2 = ui32Data - ui32B1;

		/* Copy up to end of circular buffer */
		HostMemCopy((IMG_VOID *) pui8OutBuf,
				(IMG_VOID *)((IMG_UINTPTR_T)psStream->pvBase + psStream->ui32RPtr),
				ui32B1);

		/* Copy from start of circular buffer */
		HostMemCopy((IMG_VOID *)(pui8OutBuf + ui32B1),
				psStream->pvBase,
				ui32B2);

		/* Update read pointer now that we've copied the data out */
		psStream->ui32RPtr = ui32B2;
	}
	else
	{	/* Copy data from wherever */
		HostMemCopy((IMG_VOID *) pui8OutBuf,
				(IMG_VOID *)((IMG_UINTPTR_T)psStream->pvBase + psStream->ui32RPtr),
				ui32Data);

		/* Update read pointer now that we've copied the data out */
		psStream->ui32RPtr += ui32Data;

		/* Check for wrapping */
		if (psStream->ui32RPtr == psStream->ui32Size)
		{
			psStream->ui32RPtr = 0;
		}
	}

	return(ui32Data);
}

/*!****************************************************************************
 @name		DBGDrivSetCaptureMode
 @brief		Set capture mode
 @param		psStream - stream
 @param		ui32Mode - capturing mode
 @param		ui32Start - start frame (frame mode only)
 @param		ui32End - end frame (frame mode)
 @param		ui32SampleRate - sampling frequency (frame mode)
*****************************************************************************/
void IMG_CALLCONV DBGDrivSetCaptureMode(PDBG_STREAM psStream,IMG_UINT32 ui32Mode,IMG_UINT32 ui32Start,IMG_UINT32 ui32End,IMG_UINT32 ui32SampleRate)
{
	/*
		Validate buffer.
	*/
	if (!StreamValid(psStream))
	{
		return;
	}

	psStream->psCtrl->ui32CapMode = ui32Mode;
	psStream->psCtrl->ui32DefaultMode = ui32Mode;
	psStream->psCtrl->ui32Start = ui32Start;
	psStream->psCtrl->ui32End = ui32End;
	psStream->psCtrl->ui32SampleRate = ui32SampleRate;

	/*
		Activate hotkey it the stream is of this type.
	*/
	if (psStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_HOTKEY)
	{
		ActivateHotKeys(psStream);
	}
}

/*!****************************************************************************
 @name		DBGDrivSetOutputMode
 @brief		Change output mode
 @param		psStream - stream
 @param		ui32OutMode - output mode
 @return	none
*****************************************************************************/
void IMG_CALLCONV DBGDrivSetOutputMode(PDBG_STREAM psStream,IMG_UINT32 ui32OutMode)
{
	/*
		Validate buffer.
	*/
	if (!StreamValid(psStream))
	{
		return;
	}

	psStream->psCtrl->ui32OutMode = ui32OutMode;
}

/*!****************************************************************************
 @name		DBGDrivSetDebugLevel
 @brief		Change debug level
 @param		psStream - stream
 @param		ui32DebugLevel - verbosity level
 @return	none
*****************************************************************************/
void IMG_CALLCONV DBGDrivSetDebugLevel(PDBG_STREAM psStream,IMG_UINT32 ui32DebugLevel)
{
	/*
		Validate buffer.
	*/
	if (!StreamValid(psStream))
	{
		return;
	}

	psStream->psCtrl->ui32DebugLevel = ui32DebugLevel;
}

/*!****************************************************************************
 @name		DBGDrivSetFrame
 @brief		Advance frame counter
 @param		psStream - stream
 @param		ui32Frame - frame number
 @return	none
*****************************************************************************/
void IMG_CALLCONV DBGDrivSetFrame(PDBG_STREAM psStream,IMG_UINT32 ui32Frame)
{
	/*
		Validate buffer.
	*/
	if (!StreamValid(psStream))
	{
		return;
	}

	psStream->psCtrl->ui32Current = ui32Frame;

	if ((ui32Frame >= psStream->psCtrl->ui32Start) &&
		(ui32Frame <= psStream->psCtrl->ui32End) &&
		(((ui32Frame - psStream->psCtrl->ui32Start) % psStream->psCtrl->ui32SampleRate) == 0))
	{
		psStream->psCtrl->ui32Flags |= DEBUG_FLAGS_ENABLESAMPLE;
	}
	else
	{
		psStream->psCtrl->ui32Flags &= ~DEBUG_FLAGS_ENABLESAMPLE;
	}

	if (g_bHotkeyMiddump)
	{
		if ((ui32Frame >= g_ui32HotkeyMiddumpStart) &&
			(ui32Frame <= g_ui32HotkeyMiddumpEnd) &&
			(((ui32Frame - g_ui32HotkeyMiddumpStart) % psStream->psCtrl->ui32SampleRate) == 0))
		{
			psStream->psCtrl->ui32Flags |= DEBUG_FLAGS_ENABLESAMPLE;
		}
		else
		{
			psStream->psCtrl->ui32Flags &= ~DEBUG_FLAGS_ENABLESAMPLE;
			if (psStream->psCtrl->ui32Current > g_ui32HotkeyMiddumpEnd)
			{
				g_bHotkeyMiddump = IMG_FALSE;
			}
		}
	}

	/* Check to see if hotkey press has been registered (from keyboard filter) */
	if (g_bHotKeyRegistered)
	{
		g_bHotKeyRegistered = IMG_FALSE;

		PVR_DPF((PVR_DBG_MESSAGE,"Hotkey pressed (%p)!\n",psStream));

		if (!g_bHotKeyPressed)
		{
			/*
				Capture the next frame.
			*/
			g_ui32HotKeyFrame = psStream->psCtrl->ui32Current + 2;

			/*
				Do the flag.
			*/
			g_bHotKeyPressed = IMG_TRUE;
		}

		/*
			If in framed hotkey mode, then set start frame.
		*/
		if (((psStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_FRAMED) != 0) && 
			((psStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_HOTKEY) != 0))
		{
			if (!g_bHotkeyMiddump)
			{
				/* Turn on */
				g_ui32HotkeyMiddumpStart = g_ui32HotKeyFrame + 1;
				g_ui32HotkeyMiddumpEnd = 0xffffffff;
				g_bHotkeyMiddump = IMG_TRUE;
				PVR_DPF((PVR_DBG_MESSAGE,"Sampling every %d frame(s)\n", psStream->psCtrl->ui32SampleRate));
			}
			else
			{
				/* Turn off */
				g_ui32HotkeyMiddumpEnd = g_ui32HotKeyFrame;
				PVR_DPF((PVR_DBG_MESSAGE,"Turning off sampling\n"));
			}
		}

	}

	/*
		Clear the hotkey frame indicator when over that frame.
	*/
	if (psStream->psCtrl->ui32Current > g_ui32HotKeyFrame)
	{
		g_bHotKeyPressed = IMG_FALSE;
	}
}

/*!****************************************************************************
 @name		DBGDrivGetFrame
 @brief		Retrieve current frame number
 @param		psStream - stream
 @return	frame number
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivGetFrame(PDBG_STREAM psStream)
{
	/*
		Validate buffer.
	*/
	if (!StreamValid(psStream))
	{
		return(0);
	}

	return(psStream->psCtrl->ui32Current);
}

/*!****************************************************************************
 @name		DBGDrivIsLastCaptureFrame
 @brief		Is this the last frame to be captured?
 @param		psStream - stream
 @return	true if last capture frame, false otherwise
*****************************************************************************/
IMG_BOOL IMG_CALLCONV DBGDrivIsLastCaptureFrame(PDBG_STREAM psStream)
{
	IMG_UINT32	ui32NextFrame;

	/*
		Validate buffer.
	*/
	if (!StreamValid(psStream))
	{
		return IMG_FALSE;
	}

	if (psStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_FRAMED)
	{
		ui32NextFrame = psStream->psCtrl->ui32Current + psStream->psCtrl->ui32SampleRate;
		if (ui32NextFrame > psStream->psCtrl->ui32End)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

/*!****************************************************************************
 @name		DBGDrivIsCaptureFrame
 @brief		Is this a capture frame?
 @param		psStream - stream
 @param		bCheckPreviousFrame - set if it needs to be 1 frame ahead
 @return	true if capturing this frame, false otherwise
*****************************************************************************/
IMG_BOOL IMG_CALLCONV DBGDrivIsCaptureFrame(PDBG_STREAM psStream, IMG_BOOL bCheckPreviousFrame)
{
	IMG_UINT32 ui32FrameShift = bCheckPreviousFrame ? 1UL : 0UL;

	/*
		Validate buffer.
	*/
	if (!StreamValid(psStream))
	{
		return IMG_FALSE;
	}

	if (psStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_FRAMED)
	{
		/* Needs to be one frame ahead, so disppatch can turn everything on */
		if (g_bHotkeyMiddump)
		{
			if ((psStream->psCtrl->ui32Current >= (g_ui32HotkeyMiddumpStart - ui32FrameShift)) &&
				(psStream->psCtrl->ui32Current <= (g_ui32HotkeyMiddumpEnd - ui32FrameShift)) &&
				((((psStream->psCtrl->ui32Current + ui32FrameShift) - g_ui32HotkeyMiddumpStart) % psStream->psCtrl->ui32SampleRate) == 0))
			{
				return IMG_TRUE;
			}
		}
		else
		{
			if ((psStream->psCtrl->ui32Current >= (psStream->psCtrl->ui32Start - ui32FrameShift)) &&
				(psStream->psCtrl->ui32Current <= (psStream->psCtrl->ui32End - ui32FrameShift)) &&
				((((psStream->psCtrl->ui32Current + ui32FrameShift) - psStream->psCtrl->ui32Start) % psStream->psCtrl->ui32SampleRate) == 0))
			{
				return IMG_TRUE;
			}
		}
	}
	else if (psStream->psCtrl->ui32CapMode == DEBUG_CAPMODE_HOTKEY)
	{
		if ((psStream->psCtrl->ui32Current == (g_ui32HotKeyFrame-ui32FrameShift)) && (g_bHotKeyPressed))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}


/*!****************************************************************************
 @name		DBGDrivOverrideMode
 @brief		Override capture mode
 @param		psStream - stream
 @param		ui32Mode - capture mode
 @return	none
*****************************************************************************/
void IMG_CALLCONV DBGDrivOverrideMode(PDBG_STREAM psStream,IMG_UINT32 ui32Mode)
{
	/*
		Validate buffer.
	*/
	if (!StreamValid(psStream))
	{
		return;
	}

	psStream->psCtrl->ui32CapMode = ui32Mode;
}

/*!****************************************************************************
 @name		DBGDrivDefaultMode
 @param		psStream - stream
 @return	none
*****************************************************************************/
void IMG_CALLCONV DBGDrivDefaultMode(PDBG_STREAM psStream)
{
	/*
		Validate buffer.
	*/
	if (!StreamValid(psStream))
	{
		return;
	}

	psStream->psCtrl->ui32CapMode = psStream->psCtrl->ui32DefaultMode;
}

/*!****************************************************************************
 @name		DBGDrivSetMarker
 @brief		Sets the marker in the stream to split output files
 @param		psStream, ui32Marker
 @return	nothing
*****************************************************************************/
void IMG_CALLCONV DBGDrivSetMarker(PDBG_STREAM psStream, IMG_UINT32 ui32Marker)
{
	/*
		Validate buffer
	*/
	if (!StreamValid(psStream))
	{
		return;
	}

	psStream->ui32Marker = ui32Marker;
}

/*!****************************************************************************
 @name		DBGDrivGetMarker
 @brief 	Gets the marker in the stream to split output files
 @param	 	psStream - stream
 @return	marker offset
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivGetMarker(PDBG_STREAM psStream)
{
	/*
		Validate buffer
	*/
	if (!StreamValid(psStream))
	{
		return 0;
	}

	return psStream->ui32Marker;
}


/*!****************************************************************************
 @name		DBGDrivGetStreamOffset
 @brief		Gets the amount of data written to the stream
 @param		psMainStream - stream
 @return	bytes written
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivGetStreamOffset(PDBG_STREAM psMainStream)
{
	PDBG_STREAM psStream;

	/*
		Validate buffer
	*/
	if (!StreamValid(psMainStream))
	{
		return 0;
	}

	if(psMainStream->psCtrl->bInitPhaseComplete)
	{
		psStream = psMainStream;
	}
	else
	{
		psStream = psMainStream->psInitStream;
	}

	return psStream->ui32DataWritten;
}

/*!****************************************************************************
 @name		DBGDrivSetStreamOffset
 @brief		Sets the amount of data written to the stream
 @param		psMainStream - stream
 @param		ui32StreamOffset - stream offset
 @return	Nothing
*****************************************************************************/
IMG_VOID IMG_CALLCONV DBGDrivSetStreamOffset(PDBG_STREAM psMainStream, IMG_UINT32 ui32StreamOffset)
{
	PDBG_STREAM psStream;

	/*
		Validate buffer
	*/
	if (!StreamValid(psMainStream))
	{
		return;
	}

	if(psMainStream->psCtrl->bInitPhaseComplete)
	{
		psStream = psMainStream;
	}
	else
	{
		psStream = psMainStream->psInitStream;
	}

	PVR_DPF((PVR_DBGDRIV_MESSAGE, "DBGDrivSetStreamOffset: %s set to %x b",
			psStream->szName,
			ui32StreamOffset));
	psStream->ui32DataWritten = ui32StreamOffset;
}

/*!****************************************************************************
 @name		DBGDrivGetServiceTable
 @brief		get jump table for Services driver
 @return	pointer to jump table
*****************************************************************************/
IMG_PVOID IMG_CALLCONV DBGDrivGetServiceTable(IMG_VOID)
{
	return((IMG_PVOID)&g_sDBGKMServices);
}

/*!****************************************************************************
 @name		DBGDrivWriteLF
 @brief		Store data that should only be kept from the last frame dumped
 @param		psStream - stream
 @param		pui8InBuf - input buffer
 @param		ui32InBuffSize - size
 @param		ui32Level - verbosity level
 @param		ui32Flags - flags
 @return	bytes written
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivWriteLF(PDBG_STREAM psStream, IMG_UINT8 * pui8InBuf, IMG_UINT32 ui32InBuffSize, IMG_UINT32 ui32Level, IMG_UINT32 ui32Flags)
{
	PDBG_LASTFRAME_BUFFER	psLFBuffer;
	PDBG_LASTFRAME_BUFFER	psLFBufferList;

	PVR_UNREFERENCED_PARAMETER(ui32Flags);

	/*
		Validate buffer.
	*/
	if (!StreamValidForWrite(psStream))
	{
		return(0xFFFFFFFFUL);
	}

	/*
		Check debug level.
	*/
	if ((psStream->psCtrl->ui32DebugLevel & ui32Level) == 0)
	{
		return(0xFFFFFFFFUL);
	}

	/*
		Only write data if debug mode adds up...
	*/
	if ((psStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_FRAMED) != 0)
	{
		if	(psStream->psCtrl->bInitPhaseComplete && 
			 ((psStream->psCtrl->ui32Flags & DEBUG_FLAGS_ENABLESAMPLE) == 0))
		{
			/* throw away non-capturing data */
			return(ui32InBuffSize);
		}
	}
	else if (psStream->psCtrl->ui32CapMode == DEBUG_CAPMODE_HOTKEY)
	{
		if	(psStream->psCtrl->bInitPhaseComplete && 
			 ((psStream->psCtrl->ui32Current != g_ui32HotKeyFrame) || (g_bHotKeyPressed == IMG_FALSE)))
		{
			/* throw away non-capturing data */
			return(ui32InBuffSize);
		}
	}

	if (psStream->psCtrl->bInitPhaseComplete)
	{
		psLFBufferList = g_psLFBufferList;
		PVR_DPF((PVR_DBG_ERROR, "DBGDrivWriteLF: writing to LF buffer %d bytes", ui32InBuffSize));
	}
	else
	{
		psLFBufferList = g_psLFDeinitBufferList;
	}

	psLFBuffer = FindLFBufFromList(psStream, psLFBufferList);

	/*
		Append the data to the end of the buffer
	*/
	ui32InBuffSize = ((psLFBuffer->ui32BufLen + ui32InBuffSize) > LAST_FRAME_BUF_SIZE) ? (LAST_FRAME_BUF_SIZE - psLFBuffer->ui32BufLen) : ui32InBuffSize;

	/*
		No room in the buffer?
	 */
	if(ui32InBuffSize == 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "DBGDrivWriteLF: No more space available in the LF buffer."));
		return(0xFFFFFFFFUL);
	}

	HostMemCopy((IMG_VOID *)(&psLFBuffer->ui8Buffer[psLFBuffer->ui32BufLen]), (IMG_VOID *)pui8InBuf, ui32InBuffSize);
	psLFBuffer->ui32BufLen += ui32InBuffSize;

	return(ui32InBuffSize);
}

/*!****************************************************************************
 @name		DBGDrivReadLF
 @brief		Read data that should only be kept from the last frame dumped
 @param		psStream - stream
 @param		ui32OutBuffSize - buffer size
 @param		pui8OutBuf - output buffer
 @return	bytes read
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivReadLF(PDBG_STREAM psStream, IMG_UINT32 ui32OutBuffSize, IMG_UINT8 * pui8OutBuf)
{
	PDBG_LASTFRAME_BUFFER	psLFBuffer;
	PDBG_LASTFRAME_BUFFER	psLFDeinitBuffer;
	IMG_UINT32				ui32TotalSize;

	/*
		Validate buffer.
	*/
	if (!StreamValidForRead(psStream))
	{
		return(0);
	}

	/* we need to treat LF and LF deinit as one large buffer */
	psLFBuffer = FindLFBufFromList(psStream, g_psLFBufferList);
	psLFDeinitBuffer = FindLFBufFromList(psStream, g_psLFDeinitBufferList);

	ui32TotalSize = psLFBuffer->ui32BufLen + psLFDeinitBuffer->ui32BufLen;

	/*
		Get amount of data to copy
	*/
	if (ui32OutBuffSize < ui32TotalSize)
	{
		PVR_DPF((PVR_DBG_ERROR, "DBGDrivReadLF: Not enough space (%d) to copy LF and LF deinit buffers(%d).", ui32OutBuffSize, ui32TotalSize));
		/* don't copy anything */
		ui32TotalSize = 0;
	}
	else
	{
		/*
			Copy the data into the buffer
		*/
		HostMemCopy(pui8OutBuf, psLFBuffer->ui8Buffer, psLFBuffer->ui32BufLen);

		HostMemCopy((pui8OutBuf + psLFBuffer->ui32BufLen), psLFDeinitBuffer->ui8Buffer, psLFDeinitBuffer->ui32BufLen);
	}
	
	return ui32TotalSize;

}

/*!****************************************************************************
 @name		DBGDrivStartInitPhase
 @brief		Marks start of init phase
 @param		psStream - stream
 @return	void
*****************************************************************************/
IMG_VOID IMG_CALLCONV DBGDrivStartInitPhase(PDBG_STREAM psStream)
{
	psStream->psCtrl->bInitPhaseComplete = IMG_FALSE;
}

/*!****************************************************************************
 @name		DBGDrivStopInitPhase
 @brief		Marks end of init phase
 @param		psStream - stream
 @return	void
*****************************************************************************/
IMG_VOID IMG_CALLCONV DBGDrivStopInitPhase(PDBG_STREAM psStream)
{
	psStream->psCtrl->bInitPhaseComplete = IMG_TRUE;
}

IMG_BOOL IMG_CALLCONV DBGDrivIsInitPhase(PDBG_STREAM psStream)
{
	return !psStream->psCtrl->bInitPhaseComplete;
}

#if defined(SUPPORT_DBGDRV_EVENT_OBJECTS)
/*!****************************************************************************
 @name		DBGDrivWaitForEvent
 @brief		waits for an event
 @param		eEvent - debug driver event
 @return	void
*****************************************************************************/
IMG_VOID IMG_CALLCONV DBGDrivWaitForEvent(DBG_EVENT eEvent)
{
	HostWaitForEvent(eEvent);
}
#endif

/*	Use PVR_DPF() to avoid state messages in release build */
#if defined(PVR_DISABLE_LOGGING) || !defined(DEBUG)
#define PVR_LOG(...)
#else

extern void PVRSRVDebugPrintf(IMG_UINT32	ui32DebugLevel,
						const IMG_CHAR*	pszFileName,
						IMG_UINT32	ui32Line,
						const IMG_CHAR*	pszFormat,
						...	);
/* Reproduce the PVR_LOG macro here but direct it to DPF */
#define PVR_LOG(...)	PVRSRVDebugPrintf( DBGPRIV_CALLTRACE, __FILE__, __LINE__ , __VA_ARGS__);

#endif


/*!****************************************************************************
 @name		DBGDrivGetCtrlState
 @brief		Gets a state value from the debug driver or stream
 @param		psStream - stream
 @param		ui32StateID - state ID
 @return	Nothing
*****************************************************************************/
IMG_UINT32 IMG_CALLCONV DBGDrivGetCtrlState(PDBG_STREAM psStream, IMG_UINT32 ui32StateID)
{
	/* Validate buffer */
	if (!StreamValid(psStream))
	{
		return (0xFFFFFFFF);
	}

	/* Retrieve the state asked for */
	switch (ui32StateID)
	{
	case DBG_GET_STATE_CURRENT_PAST_END:
		return (psStream->psCtrl->ui32Current > psStream->psCtrl->ui32End);
		break;
	case DBG_GET_STATE_INIT_PHASE_COMPLETE:
		return (psStream->psCtrl->bInitPhaseComplete);
		break;
	case DBG_GET_STATE_FLAG_IS_READONLY:
		return ((psStream->psCtrl->ui32Flags & DEBUG_FLAGS_READONLY) != 0);
		break;
	case DBG_GET_STATE_THROW_DATA_AWAY:
		return (((psStream->psCtrl->ui32CapMode & DEBUG_CAPMODE_FRAMED) != 0) &&
				(psStream->psCtrl->ui32Start == 0xFFFFFFFFU) &&
				(psStream->psCtrl->ui32End == 0xFFFFFFFFU) &&
				psStream->psCtrl->bInitPhaseComplete);
        break;

	case 0xFE: /* Dump the current stream state */
		PVR_LOG("------ PDUMP DBGDriv: psStream( %p ) ( %s )",
				psStream, psStream->szName);
		PVR_LOG("------ PDUMP DBGDriv: psStream->pvBase( %p ) psStream->ui32Size( %u ) psStream->bCircularAllowed( %d )",
				psStream->pvBase, psStream->ui32Size, psStream->bCircularAllowed);
		PVR_LOG("------ PDUMP DBGDriv: psStream->ui32RPtr( %u ) psStream->ui32WPtr( %u )",
				psStream->ui32RPtr, psStream->ui32WPtr);
		PVR_LOG("------ PDUMP DBGDriv: psStream->ui32DataWritten( %u ) psStream->ui32Marker( %u ) psStream->ui32InitPhaseWOff( %u )",
				psStream->ui32DataWritten, psStream->ui32Marker, psStream->ui32InitPhaseWOff);

		if (psStream->psCtrl)
		{
			PVR_LOG("------ PDUMP DBGDriv: psStream->psCtrl( %p ) psStream->psCtrl->bInitPhaseComplete( %d ) psStream->psCtrl->ui32Flags( %x )",
					psStream->psCtrl, psStream->psCtrl->bInitPhaseComplete, psStream->psCtrl->ui32Flags);
			PVR_LOG("------ PDUMP DBGDriv: psStream->psCtrl->ui32CapMode( %u ) psStream->psCtrl->ui32OutMode( %u ) psStream->psCtrl->ui32DebugLevel( %u ) psStream->psCtrl->ui32DefaultMode( %u )",
					psStream->psCtrl->ui32CapMode, psStream->psCtrl->ui32OutMode, psStream->psCtrl->ui32DebugLevel, psStream->psCtrl->ui32DefaultMode);
			PVR_LOG("------ PDUMP DBGDriv: psStream->psCtrl->ui32Start( %u ) psStream->psCtrl->ui32End( %u ) psStream->psCtrl->ui32Current( %u ) psStream->psCtrl->ui32SampleRate( %u )",
					psStream->psCtrl->ui32Start, psStream->psCtrl->ui32End, psStream->psCtrl->ui32Current, psStream->psCtrl->ui32SampleRate);
		}

		if (psStream->psInitStream)
		{
			PVR_LOG("------ PDUMP DBGDriv: psInitStream( %p ) ( %s )",
					psStream->psInitStream, psStream->psInitStream->szName);
			PVR_LOG("------ PDUMP DBGDriv: psInitStream->pvBase( %p ) psInitStream->ui32Size( %u ) psInitStream->bCircularAllowed( %d )",
					psStream->psInitStream->pvBase, psStream->psInitStream->ui32Size, psStream->psInitStream->bCircularAllowed);
			PVR_LOG("------ PDUMP DBGDriv: psInitStream->ui32RPtr( %u ) psInitStream->ui32WPtr( %u )",
					psStream->psInitStream->ui32RPtr, psStream->psInitStream->ui32WPtr);
			PVR_LOG("------ PDUMP DBGDriv: psInitStream->ui32DataWritten( %u ) psInitStream->ui32Marker( %u ) psInitStream->ui32InitPhaseWOff( %u ) ",
					psStream->psInitStream->ui32DataWritten, psStream->psInitStream->ui32Marker, psStream->psInitStream->ui32InitPhaseWOff);
		}

		if (psStream->psInitStream && psStream->psInitStream->psCtrl)
		{
			PVR_LOG("------ PDUMP DBGDriv: psInitStream->psCtrl( %p ) psInitStream->psCtrl->bInitPhaseComplete( %d ) psInitStream->psCtrl->ui32Flags( %x )",
					psStream->psInitStream->psCtrl, psStream->psInitStream->psCtrl->bInitPhaseComplete, psStream->psInitStream->psCtrl->ui32Flags);
			PVR_LOG("------ PDUMP DBGDriv: psInitStream->psCtrl->ui32CapMode( %u ) psInitStream->psCtrl->ui32OutMode( %u ) psInitStream->psCtrl->ui32DebugLevel( %u ) psInitStream->psCtrl->ui32DefaultMode( %u )",
					psStream->psInitStream->psCtrl->ui32CapMode, psStream->psInitStream->psCtrl->ui32OutMode, psStream->psInitStream->psCtrl->ui32DebugLevel, psStream->psInitStream->psCtrl->ui32DefaultMode);
			PVR_LOG("------ PDUMP DBGDriv: psInitStream->psCtrl->ui32Start( %u ) psInitStream->psCtrl->ui32End( %u ) psInitStream->psCtrl->ui32Current( %u ) psInitStream->psCtrl->ui32SampleRate( %u )",
					psStream->psInitStream->psCtrl->ui32Start, psStream->psInitStream->psCtrl->ui32End, psStream->psInitStream->psCtrl->ui32Current, psStream->psInitStream->psCtrl->ui32SampleRate);
		}
		break;

	case 0xFF: /* Dump driver state not in a stream */
		{
			PDBG_LASTFRAME_BUFFER psLF;

			PVR_LOG("------ PDUMP DBGDriv: g_psStreamList( head %p ) g_pvAPIMutex( %p )", g_psStreamList, g_pvAPIMutex);
			PVR_LOG("------ PDUMP DBGDriv LF: g_psLFBufferList( head %p ), g_psLFDeinitBufferList( head %p )", g_psLFBufferList, g_psLFDeinitBufferList);
			psLF = g_psLFBufferList;
			while (psLF)
			{
				PVR_LOG("------ PDUMP DBGDriv LF: psLF( %p ) psLF->psStream( %p )",
						psLF, psLF->psStream);
				PVR_LOG("------ PDUMP DBGDriv LF: psLF->ui32BufLen( %u ) psLF->psNext( %p )",
						psLF->ui32BufLen, psLF->psNext);
				psLF = psLF->psNext;
			}

			psLF = g_psLFDeinitBufferList;
			while (psLF)
			{
				PVR_LOG("------ PDUMP DBGDriv LF Deinit: psLF( %p ) psLF->psStream( %p )",
						psLF, psLF->psStream);
				PVR_LOG("------ PDUMP DBGDriv LF Deinit: psLF->ui32BufLen( %u ) psLF->psNext( %p )",
						psLF->ui32BufLen, psLF->psNext);
				psLF = psLF->psNext;
			}

			PVR_LOG("------ PDUMP DBGDriv: g_ui32LOff( %u ) g_ui32Line( %u ) g_ui32MonoLines( %u )", g_ui32LOff, g_ui32Line, g_ui32MonoLines);
			PVR_LOG("------ PDUMP DBGDriv: g_bHotkeyMiddump( %d ) g_ui32HotkeyMiddumpStart( %u ) g_ui32HotkeyMiddumpEnd( %u )", g_bHotkeyMiddump, g_ui32HotkeyMiddumpStart, g_ui32HotkeyMiddumpEnd);
			PVR_LOG("------ PDUMP DBGDriv: g_ui32HotKeyFrame( %u ) g_bHotKeyPressed( %d ) g_bHotKeyRegistered( %d )", g_ui32HotKeyFrame, g_bHotKeyPressed, g_bHotKeyRegistered);
			PVR_LOG("------ PDUMP DBGDriv: g_fnDBGKMNotifier.pfnConnectNotifier( %p )", g_fnDBGKMNotifier.pfnConnectNotifier);
		}
		break;

	default:
		PVR_ASSERT(0);
	}

	return (0xFFFFFFFF);
}

/*!****************************************************************************
 @name		ExpandStreamBuffer
 @brief		allocates a new buffer when the current one is full
 @param		psStream - stream
 @param		ui32NewSize - new size
 @return	IMG_TRUE - if allocation succeeded, IMG_FALSE - if not
*****************************************************************************/
IMG_BOOL ExpandStreamBuffer(PDBG_STREAM psStream, IMG_UINT32 ui32NewSize)
{
	IMG_VOID *	pvNewBuf;
	IMG_UINT32	ui32NewSizeInPages;
	IMG_UINT32	ui32NewWOffset;
	IMG_UINT32	ui32NewROffset;
	IMG_UINT32	ui32SpaceInOldBuf;

	/* 
		First check new size is bigger than existing size 
	*/
	if (psStream->ui32Size >= ui32NewSize)
	{
		return IMG_FALSE;
	}

	/*
		Calc space in old buffer 
	*/
	ui32SpaceInOldBuf = SpaceInStream(psStream);

	/*
		Allocate new buffer 
	*/
	ui32NewSizeInPages = ((ui32NewSize + 0xfffUL) & ~0xfffUL) / 4096UL;

	if ((psStream->psCtrl->ui32Flags & DEBUG_FLAGS_USE_NONPAGED_MEM) != 0)
	{
		pvNewBuf = HostNonPageablePageAlloc(ui32NewSizeInPages);
	}
	else
	{
		pvNewBuf = HostPageablePageAlloc(ui32NewSizeInPages);
	}

	if (pvNewBuf == IMG_NULL)
	{
		return IMG_FALSE;
	}

	if(psStream->bCircularAllowed)
	{
		/*
			Copy over old buffer to new one, we place data at start of buffer
			even if Read offset is not at start of buffer
		*/
		if (psStream->ui32RPtr <= psStream->ui32WPtr)
		{
			/*
				No wrapping of data so copy data to start of new buffer 
			*/
		HostMemCopy(pvNewBuf,
					(IMG_VOID *)((IMG_UINTPTR_T)psStream->pvBase + psStream->ui32RPtr),
					psStream->ui32WPtr - psStream->ui32RPtr);
		}
		else
		{
			IMG_UINT32	ui32FirstCopySize;
	
			/*
				The data has wrapped around the buffer, copy beginning of buffer first 
			*/
			ui32FirstCopySize = psStream->ui32Size - psStream->ui32RPtr;
	
			HostMemCopy(pvNewBuf,
					(IMG_VOID *)((IMG_UINTPTR_T)psStream->pvBase + psStream->ui32RPtr),
					ui32FirstCopySize);
	
			/*
				Now second half 
			*/
			HostMemCopy((IMG_VOID *)((IMG_UINTPTR_T)pvNewBuf + ui32FirstCopySize),
					(IMG_VOID *)(IMG_PBYTE)psStream->pvBase,
					psStream->ui32WPtr);
		}
		ui32NewROffset = 0;
	}
	else
	{
		/* Copy everything in the old buffer to the new one */
		HostMemCopy(pvNewBuf, psStream->pvBase,	psStream->ui32WPtr);
		ui32NewROffset = psStream->ui32RPtr;
	}

	/*
		New Write offset is at end of data 
	*/                                                        
	ui32NewWOffset = psStream->ui32Size - ui32SpaceInOldBuf;

	/*
		Free old buffer 
	*/
	if ((psStream->psCtrl->ui32Flags & DEBUG_FLAGS_USE_NONPAGED_MEM) != 0)
	{
		HostNonPageablePageFree(psStream->pvBase);
	}
	else
	{
		HostPageablePageFree(psStream->pvBase);
	}

	/*
		Now set new params up 
	*/
	psStream->pvBase = pvNewBuf;
	psStream->ui32RPtr = ui32NewROffset;
	psStream->ui32WPtr = ui32NewWOffset;
	psStream->ui32Size = ui32NewSizeInPages * 4096;

	return IMG_TRUE;
}

/*!****************************************************************************
 @name		SpaceInStream
 @brief		remaining space in stream
 @param		psStream - stream
 @return	bytes remaining
*****************************************************************************/
IMG_UINT32 SpaceInStream(PDBG_STREAM psStream)
{
	IMG_UINT32	ui32Space;

	if (psStream->bCircularAllowed)
	{
		/* Allow overwriting the buffer which was already read */
	if (psStream->ui32RPtr > psStream->ui32WPtr)
	{
		ui32Space = psStream->ui32RPtr - psStream->ui32WPtr;
	}
	else
	{
		ui32Space = psStream->ui32RPtr + (psStream->ui32Size - psStream->ui32WPtr);
	}
	}
	else
	{
		/* Don't overwrite anything */
		ui32Space = psStream->ui32Size - psStream->ui32WPtr;
	}

	return ui32Space;
}


/*!****************************************************************************
 @name		DestroyAllStreams
 @brief		delete all streams in list
 @return	none
*****************************************************************************/
void DestroyAllStreams(void)
{
	while (g_psStreamList != IMG_NULL)
	{
		DBGDrivDestroyStream(g_psStreamList);
	}
	return;
}

/*!****************************************************************************
 @name		FindStreamBufFromList
 @brief		Finds the buffer of the given stream in the list
 @param		psStream - stream to find
 @param		psStream - stream to find
 @return	stream if found, NULL otherwise
*****************************************************************************/
PDBG_LASTFRAME_BUFFER FindLFBufFromList(PDBG_STREAM psStream, PDBG_LASTFRAME_BUFFER	psLFBuffer)
{
	while (psLFBuffer)
	{
		if (psLFBuffer->psStream == psStream)
		{
			break;
		}

		psLFBuffer = psLFBuffer->psNext;
	}

	return psLFBuffer;
}

/******************************************************************************
 End of file (DBGDRIV.C)
******************************************************************************/
