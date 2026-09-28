// --------------------------------------------------------------
//
//  Thinkpad Fan Control
//
// --------------------------------------------------------------
//
//	This program and source code is in the public domain.
//
// --------------------------------------------------------------
//
//  State shared between the instance that owns the EC (the engine)
//  and the user interface clients attached to it.
//
// --------------------------------------------------------------

#ifndef SHAREDSTATE_H
#define SHAREDSTATE_H

#pragma once

struct FCSHARED {
	volatile LONG stateSeq;   // bumped by the engine after every publish
	volatile LONG cmdSeq;     // bumped by a client after every command
	volatile LONG ackSeq;     // cmdSeq the engine has already acted on

	int mode;                 // 1 bios, 2 smart, 3 manual, 4 gorilla (Gorilla fork)
	int smartLevel;           // active smart profile, 0 or 1
	int fanCtrl;              // raw contents of EC register 0x2f
	int fan1lo, fan1hi;
	int fan2lo, fan2hi;
	int sensors[12];

	int cmdMode;              // mode the user picked
	int cmdSmart;             // smart profile the user picked, -1 for none
	char cmdLevelText[16];    // manual level as typed, parsed by the engine

	// Gorilla mode (Gorilla fork): the engine publishes what it runs, a
	// client asks for a change; -1 in a cmd field means "no change"
	int gorillaTarget;        // CPU target in C (40/45/50)
	int gorillaFloor;         // lowest fan level Gorilla mode may use
	int cmdGorillaTarget;
	int cmdGorillaFloor;

	volatile LONG traceSeq;   // bumped by the engine for every line below
	char traceLines[48][160]; // ring of engine log lines, mirrored by clients
};

bool SharedState_Create();   // engine side, fails if it cannot be created
bool SharedState_Attach();   // client side, fails if no engine published one
void SharedState_Close();

// a section dies with its last handle, so a killed engine leaves nothing stale
bool SharedState_EngineRunning();

FCSHARED* SharedState();     // NULL until created or attached

#endif // SHAREDSTATE_H
