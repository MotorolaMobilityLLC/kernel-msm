/* 
 * File:   Log.h
 * Author: W0017688
 *
 * Created on July 13, 2015, 10:46 AM
 * 
 * Creates a circular queue to track state transitions and times
 * Log does not overwrite if full - entries are dropped
 */
#ifdef FSC_DEBUG

#ifndef FSC_LOG_H
#define	FSC_LOG_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "platform.h"

#define LOG_SIZE 512

	typedef struct {
		FSC_U16 state;
		FSC_U16 time_ms;
		FSC_U16 time_s;
	} StateLogEntry;

	typedef struct {
		StateLogEntry logQueue[LOG_SIZE];
		FSC_U8 Start;
		FSC_U8 End;
		FSC_U8 Count;
	} StateLog;

	void InitializeStateLog(StateLog * log);
	FSC_BOOL WriteStateLog(StateLog * log, FSC_U16 state, FSC_U16 time_ms,
			       FSC_U16 time_s);
	FSC_BOOL ReadStateLog(StateLog * log, FSC_U16 * state,
			      FSC_U16 * time_ms, FSC_U16 * time_s);
	FSC_BOOL IsStateLogFull(StateLog * log);
	FSC_BOOL IsStateLogEmpty(StateLog * log);
	void DeleteStateLog(StateLog * log);

#ifdef	__cplusplus
}
#endif				// __cplusplus
#endif				/* FSC_LOG_H */
#endif				// FSC_DEBUG
