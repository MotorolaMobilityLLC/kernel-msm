#include "Log.h"

void InitializeStateLog(StateLog * log)
{
	log->Count = 0;
	log->End = 0;
	log->Start = 0;
}

BOOL WriteStateLog(StateLog * log, UINT16 state, UINT16 time_ms, UINT16 time_s)
{
	if (!IsStateLogFull(log)) {
		UINT8 index = log->End;
		log->logQueue[index].state = state;
		log->logQueue[index].time_ms = time_ms;
		log->logQueue[index].time_s = time_s;

		log->End += 1;
		if (log->End == LOG_SIZE) {
			log->End = 0;
		}

		log->Count += 1;

		return TRUE;
	} else {
		return FALSE;
	}
}

BOOL ReadStateLog(StateLog * log, UINT16 * state, UINT16 * time_ms, UINT16 * time_s)	// Read first log and delete entry
{
	if (!IsStateLogEmpty(log)) {
		UINT8 index = log->Start;
		*state = log->logQueue[index].state;
		*time_ms = log->logQueue[index].time_ms;
		*time_s = log->logQueue[index].time_s;

		log->Start += 1;
		if (log->Start == LOG_SIZE) {
			log->Start = 0;
		}

		log->Count -= 1;
		return TRUE;
	} else {
		return FALSE;
	}
}

BOOL IsStateLogFull(StateLog * log)
{
	return (log->Count == LOG_SIZE);
}

BOOL IsStateLogEmpty(StateLog * log)
{
	return (!log->Count);
}

void DeleteStateLog(StateLog * log)
{
}
