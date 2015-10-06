/* 
 * File:   Log.h
 * Author: W0017688
 *
 * Created on July 13, 2015, 10:46 AM
 * 
 * Creates a circular queue to track state transitions and times
 * Log does not overwrite if full - entries are dropped
 */

#ifndef FSC_LOG_H
#define	FSC_LOG_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "platform.h"

#define LOG_SIZE 64

	typedef struct {
		UINT16 state;
		UINT16 time_ms;
		UINT16 time_s;
	} StateLogEntry;

	typedef struct {
		StateLogEntry logQueue[LOG_SIZE];
		UINT8 Start;
		UINT8 End;
		UINT8 Count;
	} StateLog;

	void InitializeStateLog(StateLog * log);
	BOOL WriteStateLog(StateLog * log, UINT16 state, UINT16 time_ms,
			   UINT16 time_s);
	BOOL ReadStateLog(StateLog * log, UINT16 * state, UINT16 * time_ms,
			  UINT16 * time_s);
	BOOL IsStateLogFull(StateLog * log);
	BOOL IsStateLogEmpty(StateLog * log);
	void DeleteStateLog(StateLog * log);

#ifdef	__cplusplus
}
#endif
#endif				/* FSC_LOG_H */
