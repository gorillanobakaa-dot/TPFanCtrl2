// --------------------------------------------------------------
//
//  Thinkpad Fan Control
//
// --------------------------------------------------------------
//
//	This program and source code is in the public domain.
//
//	The author claims no copyright, copyleft, license or
//	whatsoever for the program itself (with exception of
//	WinIO driver).  You may use, reuse or distribute it's 
//	binaries or source code in any desired way or form,  
//	Useage of binaries or source shall be entirely and 
//	without exception at your own risk. 
// 
// --------------------------------------------------------------

#include "_prec.h"
#include "fancontrol.h"
#include "tools.h"
#include "portaccess.h"
#include "sharedstate.h"

extern bool g_clientMode;

constexpr auto TP_ECOFFSET_FAN		  = (char)0x2F;    // 1 byte (binary xyzz zzz);
constexpr auto TP_ECOFFSET_FANSPEED	  = (char)0x84;    // 16 bit word, lo/hi byte;
constexpr auto TP_ECOFFSET_TEMP0	  = (char)0x78;	 // 8 temp sensor bytes from here;
constexpr auto TP_ECOFFSET_TEMP1	  = (char)0xC0;	 // 4 temp sensor bytes from here;
constexpr auto TP_ECOFFSET_FAN_SWITCH = (char)0x31;
constexpr auto TP_ECVALUE_SELFAN1	  = (char)0x0000;
constexpr auto TP_ECVALUE_SELFAN2	  = (char)0x0001;

//-------------------------------------------------------------------------
//  switch fan according to settings
//-------------------------------------------------------------------------
bool FANCONTROL::HandleData(void) {
	char obuf[256] = "",
		 obuf2[128] = "",
		 templist[256] = "",
		 templist2[512],
		 manlevel[16] = "",
		 title2[128] = "";
	int i, maxtemp, imaxtemp;
	bool ok = 0;

	//
	// determine highest temp.
	//

	// build a list of sensors to ignore, separated by "|", e.g. "|XC1|BAT|CPU|"
	char what[16], list[128];
	sprintf_s(list, sizeof(list), "|%s|", this->IgnoreSensors);
	for (i = 0; list[i] != '\0'; i++) {
		if (list[i] == ',')
			list[i] = '|';
	}

	maxtemp = 0;
	imaxtemp = 0;
	int senstemp;
	for (i = 0; i < 12; i++) {
		sprintf_s(what, sizeof(what), "|%s|", this->State.SensorName[i]); // name (e.g. "|CPU|") to match against list above

		if (this->State.Sensors[i] != 0x80 && this->State.Sensors[i] != 0x00 && strstr(list, what) == 0) {
			int isens = this->State.Sensors[i];

			// a register that keeps jumping between the same two values holds no
			// temperature. A real sensor never repeats the exact endpoints of a
			// twenty degree leap, so three identical round trips quarantine it.
			if (this->ErraticSensorGuard) {
				int prev = this->SensorPrev[i];
				if (prev && !this->SensorErratic[i] && abs(isens - prev) >= 20) {
					int lo = __min(prev, isens), hi = __max(prev, isens);
					if (lo == this->SensorJumpLo[i] && hi == this->SensorJumpHi[i]) {
						if (++this->SensorFlaps[i] >= 3) {
							this->SensorErratic[i] = 1;
							if (!g_clientMode) {
								char ebuf[96];
								sprintf_s(ebuf, sizeof(ebuf), "Sensor %s jumps between %d and %d, ignoring it",
									this->State.SensorName[i], lo, hi);
								this->Trace(ebuf);
							}
						}
					}
					else {
						this->SensorJumpLo[i] = lo;
						this->SensorJumpHi[i] = hi;
						this->SensorFlaps[i] = 1;
					}
				}
				this->SensorPrev[i] = isens;

				if (this->SensorErratic[i])
					continue;
			}

			int ioffs = this->SensorOffset[i].offs;

			// do not apply offset if inside of temp range
			int calcTemp = isens - SensorOffset[i].offs;
			if (isens >= SensorOffset[i].hystMin && isens <= SensorOffset[i].hystMax)
				ioffs = 0;

			if (ShowBiasedTemps)
				senstemp = isens - ioffs;
			else
				senstemp = isens;

			if (senstemp < 128) {
				maxtemp = __max(senstemp, maxtemp);
				if (maxtemp <= senstemp)
					imaxtemp = i;
			}
		}
	}

	this->MaxTemp = maxtemp;
	this->iMaxTemp = imaxtemp;

	//
	// update dialog elements
	//

	// title string (for minimized window)
	if (Fahrenheit)
		sprintf_s(title2, sizeof(title2), "%d° F", this->MaxTemp * 9 / 5 + 32);
	else
		sprintf_s(title2, sizeof(title2), "%d° C", this->MaxTemp);

	// display fan state
	int fanctrl = this->State.FanCtrl;
	fanctrl2 = fanctrl;

	if (this->SlimDialog == 1) {
		sprintf_s(obuf2, sizeof(obuf2), "Fan %d ", fanctrl);
		if (fanctrl & 0x80) {
			if (!(SlimDialog && StayOnTop))
				strcat_s(obuf2, sizeof(obuf2), "(= BIOS)");
			strcat_s(title2, sizeof(title2), " Default Fan");
		}
		else {
			if (!(SlimDialog && StayOnTop))
				sprintf_s(obuf2 + strlen(obuf2), sizeof(obuf2) - strlen(obuf2), " Non Bios");
			sprintf_s(title2 + strlen(title2), sizeof(title2) - strlen(title2), " Fan %d (%s)", (fanctrl & 0x40) ? 64 : fanctrl & 0x3F, this->CurrentModeFromDialog() == 2 ? "Smart" : "Fixed");
		}
	}
	else {
		sprintf_s(obuf2, sizeof(obuf2), "0x%02x (", fanctrl);
		if (fanctrl & 0x80) {
			strcat_s(obuf2, sizeof(obuf2), "BIOS Controlled)");
			strcat_s(title2, sizeof(title2), " Default Fan");
		}
		else {
			sprintf_s(obuf2 + strlen(obuf2), sizeof(obuf2) - strlen(obuf2), "Fan Level %d, Non Bios)", (fanctrl & 0x40) ? 64 : fanctrl & 0x3F);
			sprintf_s(title2 + strlen(title2), sizeof(title2) - strlen(title2), " Fan %d (%s)", (fanctrl & 0x40) ? 64 : fanctrl & 0x3F, this->CurrentModeFromDialog() == 2 ? "Smart" : "Fixed");
		}
	}

	::SetDlgItemText(this->hwndDialog, 8100, obuf2);

	strcpy_s(this->Title2, sizeof(this->Title2), title2);

	// display fan speeds
	this->lastfan1speed = this->fan1speed;
	this->fan1speed = (this->State.Fan1SpeedHi << 8) | this->State.Fan1SpeedLo;
	if (this->fan1speed > 0x1fff)
		fan1speed = lastfan1speed;

	this->lastfan2speed = this->fan2speed;
	this->fan2speed = (this->State.Fan2SpeedHi << 8) | this->State.Fan2SpeedLo;
	if (this->fan2speed > 0x1fff)
		fan2speed = lastfan2speed;

	this->ShowFanSpeed();

	// Switch: the temperature the mode decides on. Gorilla and Compile steer by the CPU sensor; the hottest
	// sensor here can be one that never moves (the L15 Gen 3's "pwr" sits at 66 C), which looked like the fan's
	// reason while it was not (owner's screenshot, 2026-10-27)
	int shown = this->MaxTemp;
	if ((this->CurrentMode == 4 || this->CurrentMode == 5) && this->State.Sensors[0] > 0 && this->State.Sensors[0] < 128)
		shown = this->State.Sensors[0];
	if (Fahrenheit)
		sprintf_s(obuf2, sizeof(obuf2), "%d° F", shown * 9 / 5 + 32);
	else
		sprintf_s(obuf2, sizeof(obuf2), "%d° C", shown);

	::SetDlgItemText(this->hwndDialog, 8103, obuf2);

	strcpy_s(templist2, sizeof(templist2), "");

	for (i = 0; i < 12; i++) {
		int temp = this->State.Sensors[i];

		if (temp != 0 && temp < 128) {
			if (Fahrenheit)
				sprintf_s(obuf2, sizeof(obuf2), "%d° F", temp * 9 / 5 + 32);
			else
				sprintf_s(obuf2, sizeof(obuf2), "%d° C", temp);

			if (SlimDialog && StayOnTop)
				sprintf_s(templist2 + strlen(templist2), sizeof(templist2) - strlen(templist2), "%d %s %s", i + 1, this->State.SensorName[i], obuf2);
			else
				sprintf_s(templist2 + strlen(templist2), sizeof(templist2) - strlen(templist2), "%d %s %s (0x%02x)", i + 1, this->State.SensorName[i], obuf2, this->State.SensorAddr[i]);

			strcat_s(templist2, sizeof(templist2), "\r\n");
		}
		else {
			if (this->ShowAll == 1) {
				sprintf_s(obuf2, sizeof(obuf2), "n/a");

				size_t strlen_templist = strlen_s(templist2, sizeof(templist2));

				if (SlimDialog && StayOnTop)
					sprintf_s(templist2 + strlen_templist, sizeof(templist2) - strlen_templist, "%d %s %s", i + 1, this->State.SensorName[i], obuf2);
				else
					sprintf_s(templist2 + strlen_templist, sizeof(templist2) - strlen_templist, "%d %s %s (0x%02x)", i + 1, this->State.SensorName[i], obuf2, this->State.SensorAddr[i]);

				strcat_s(templist2, sizeof(templist2), "\r\n");
			}
		}
	}

	if (SlimDialog)
		::SetDlgItemText(this->hwndDialog, 8101, templist2);
	else
		this->UpdateTempDisplay();

	this->icontemp = this->State.Sensors[iMaxTemp];

	// compact single line status (combined)
	strcpy_s(templist, sizeof(templist), "");

	if (Fahrenheit) {
		for (i = 0; i < 12; i++) {
			if (this->State.Sensors[i] < 128) {
				if (this->State.Sensors[i] != 0)
					sprintf_s(templist + strlen(templist), sizeof(templist) - strlen(templist), "%d;", this->State.Sensors[i] * 9 / 5 + 32);
				else
					sprintf_s(templist + strlen(templist), sizeof(templist) - strlen(templist), "%d;", 0);
			}
			else {
				strcat_s(templist, sizeof(templist), "0;");
			}
		}
	}
	else {
		for (i = 0; i < 12; i++) {
			if (this->State.Sensors[i] != 128) {
				sprintf_s(templist + strlen(templist), sizeof(templist) - strlen(templist), "%d; ", this->State.Sensors[i]);
			}
			else {
				strcat_s(templist, sizeof(templist), "0; ");
			}
		}
	}

	templist[strlen(templist) - 1] = '\0';

	if (Fahrenheit)
		sprintf_s(CurrentStatus, sizeof(CurrentStatus), "Fan: 0x%02x / Switch: %d° F (%s)", State.FanCtrl, MaxTemp * 9 / 5 + 32, templist);
	else
		sprintf_s(CurrentStatus, sizeof(CurrentStatus), "Fan: 0x%02x / Switch: %d° C (%s)", State.FanCtrl, MaxTemp, templist);

	// display fan speed

	if (fan1speed > 0x1fff)
		fan1speed = lastfan1speed;
	if (fan2speed > 0x1fff)
		fan2speed = lastfan2speed;
	sprintf_s(obuf2, sizeof(obuf2), "%d/%d", this->fan1speed, this->fan2speed);

	sprintf_s(CurrentStatuscsv, sizeof(CurrentStatuscsv), "%s %s; %d; %d; ", templist, obuf2, State.FanCtrl, MaxTemp);

	// Compile mode and the status file need the load and the EC reading every sample (engine only)
	if (!g_clientMode) {
		this->SampleLoad();
		this->WriteStatusFile();
	}

	// a rewrite clears the selection, so the line could never be copied: hold it
	// still while the user has clicked into it, and update again once they leave
	if (::GetFocus() != ::GetDlgItem(this->hwndDialog, 8112))
		::SetDlgItemText(this->hwndDialog, 8112, this->CurrentStatus);

	//
	// handle fan control according to mode
	//

	// everything below writes the fan register, which a client never does
	this->ShowAllFromDialog();
	if (g_clientMode)
		return true;

	// a client leaves the user's choice here for the engine to act on
	FCSHARED* shared = SharedState();
	if (shared && shared->cmdSeq != this->LastCmdSeq) {
		this->LastCmdSeq = shared->cmdSeq;

		if (shared->cmdSmart == 0 || shared->cmdSmart == 1)
			this->SwitchSmartLevel(shared->cmdSmart);
		else if (shared->cmdMode >= 1 && shared->cmdMode <= 5) {      // 5 = Compile (was refused from the window)
			// an empty box would parse as level 0, the fan off
			if (shared->cmdMode == 3 && shared->cmdLevelText[0]) {
				char* end = NULL;
				long lv = strtol(shared->cmdLevelText, &end, 0);
				if (end != shared->cmdLevelText)
					this->SelectLevelInCombo(8310, (int)lv);
				else
					::SetDlgItemText(this->hwndDialog, 8310, shared->cmdLevelText);
			}
			// Gorilla settings travel with every command (-1 = unchanged)
			if (shared->cmdGorillaTarget >= 30 && shared->cmdGorillaTarget <= 70)
				this->GorillaTarget = shared->cmdGorillaTarget;
			if ((shared->cmdGorillaFloor >= 1 && shared->cmdGorillaFloor <= 7) || shared->cmdGorillaFloor == 64)
				this->GorillaFloor = shared->cmdGorillaFloor;
			this->GorillaToDialog();
			this->ModeToDialog(shared->cmdMode);
			// the engine owns the config file: remember the client's choice here
			this->PersistUserMode(shared->cmdMode, shared->cmdLevelText);
			if (shared->cmdMode == 4)
				this->PersistGorilla(this->GorillaTarget, this->GorillaFloor);
		}
	}

	this->CurrentModeFromDialog();

	if (shared) {
		shared->ackSeq = this->LastCmdSeq;
		shared->mode = this->CurrentMode;
		shared->smartLevel = this->IndSmartLevel;
		shared->gorillaTarget = this->GorillaTarget;
		shared->gorillaFloor = this->GorillaFloor;
		shared->loadPct = this->LoadPct;
		shared->fanCtrl = (unsigned char)this->State.FanCtrl;
		shared->fan1lo = (unsigned char)this->State.Fan1SpeedLo;
		shared->fan1hi = (unsigned char)this->State.Fan1SpeedHi;
		shared->fan2lo = (unsigned char)this->State.Fan2SpeedLo;
		shared->fan2hi = (unsigned char)this->State.Fan2SpeedHi;
		for (i = 0; i < 12; i++)
			shared->sensors[i] = (unsigned char)this->State.Sensors[i];
		::InterlockedIncrement(&shared->stateSeq);
	}

	switch (this->CurrentMode) {

	case 1: // BIOS
		if (this->PreviousMode != this->CurrentMode) {
			sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Change Mode from ");

			if (this->PreviousMode == 1)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "BIOS->");
			if (this->PreviousMode == 2)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart->");
			if (this->PreviousMode == 3)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Manual->");
			if (this->PreviousMode == 4)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Gorilla->");

			if (this->CurrentMode == 1)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "BIOS, setting fan speed");
			if (this->CurrentMode == 2)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart, recalculate fan speed");
			if (this->CurrentMode == 3)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Manual, setting fan speed");

			this->Trace(obuf);
		}

		if (this->State.FanCtrl != 0x080)
			ok = this->SetFan("BIOS", 0x80);
		break;

	case 2: // Smart
		this->SmartControl();
		break;

	case 3: // Manual
		if (this->PreviousMode != this->CurrentMode) {
			sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Change Mode from ");

			if (this->PreviousMode == 1)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "BIOS->");
			if (this->PreviousMode == 2)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart->");
			if (this->PreviousMode == 3)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Manual->");
			if (this->PreviousMode == 4)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Gorilla->");

			if (this->CurrentMode == 1)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "BIOS, setting fan speed");
			if (this->CurrentMode == 2)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart, recalculate fan speed");
			if (this->CurrentMode == 3)
				sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Manual, setting fan speed");

			this->Trace(obuf);
		}

		if (SlimDialog)
				::GetDlgItemText(this->hwndDialog, 8310, manlevel, sizeof(manlevel));
			else
				::GetWindowTextA(::GetDlgItem(this->hwndDialog, 8310), manlevel, sizeof(manlevel));

		int speedVal;

		if (manlevel[0] == 'x' && manlevel[1] == '\'') {
			speedVal = strtol(manlevel + 2, NULL, 16);
		} else if (manlevel[0] == '0' && (manlevel[1] == 'x' || manlevel[1] == 'X')) {
			speedVal = strtol(manlevel, NULL, 16);
		} else {
			speedVal = strtol(manlevel, NULL, 0);
		}

		if (speedVal >= 0 && speedVal <= 255) {
			// same staircase as smart mode: run 7 regulated until the fan is
			// spun up, then disengage for the last few hundred rpm
			if (speedVal == 64 && this->State.FanCtrl != 64 && this->State.FanCtrl != 7 && this->fan1speed < 4000)
				speedVal = 7;

			if (this->State.FanCtrl != speedVal)
				ok = this->SetFan("Manual", speedVal);
			else
				ok = true;
		}

		break;

	case 4: // Gorilla (Gorilla fork)
		if (this->PreviousMode != this->CurrentMode) {
			static const char* names[] = { "", "BIOS", "Smart", "Manual", "Gorilla", "Compile" };
			int pm = (this->PreviousMode >= 1 && this->PreviousMode <= 5) ? this->PreviousMode : 0;
			sprintf_s(obuf, sizeof(obuf), "Change Mode from %s->Gorilla: keep the CPU at %d C, never below level %s",
				names[pm], this->GorillaTarget, this->GorillaFloor == 64 ? "full" : std::to_string(this->GorillaFloor).c_str());
			this->Trace(obuf);
		}
		this->GorillaControl();
		ok = true;
		break;

	case 5: // Compile (Gorilla fork): Firefox and kernel builds
		if (this->PreviousMode != this->CurrentMode) {
			static const char* names[] = { "", "BIOS", "Smart", "Manual", "Gorilla", "Compile" };
			int pm = (this->PreviousMode >= 1 && this->PreviousMode <= 5) ? this->PreviousMode : 0;
			sprintf_s(obuf, sizeof(obuf), "Change Mode from %s->Compile: CPU target %d C, fan follows load and rate of rise, never below level 4",
				names[pm], this->CompileTarget);
			this->Trace(obuf);
		}
		this->CompileControl();
		ok = true;
		break;
	}

	this->PreviousMode = this->CurrentMode;

	if (this->CurrentMode == 3 && this->MaxTemp > this->ManModeExitInternal)
		this->CurrentMode = 2;

	return ok;
}

//-------------------------------------------------------------------------
//  Gorilla mode (Gorilla fork): cooling first, noise ignored
//
//  Steers by the CPU sensor (sensor 1, "cpu"), not the hottest sensor: on
//  some ThinkPads a sensor reads a fixed value regardless of the fan (the L15
//  Gen 3's "pwr" reads 66 C even at full speed while the CPU is at 45-50), and
//  steering by it would pin the fan without cooling anything. If the CPU
//  sensor does not read, the hottest sensor is used instead.
//
//    CPU >= target            full speed (0x40)
//    CPU >= target - 5        level 7
//    otherwise                GorillaFloor (never lower)
//    any sensor > ManModeExit full speed, whatever the settings
//
//  Steps down only 2 C below the threshold that raised the fan, so it does
//  not hunt. Full speed uses the same staircase as Smart and Manual.
//-------------------------------------------------------------------------
void FANCONTROL::GorillaControl(void) {
	int cpu = this->State.Sensors[0];
	int t = (cpu > 0 && cpu < 128) ? cpu : this->MaxTemp;
	int target = this->GorillaTarget;
	int floor = this->GorillaFloor;
	int cur = this->State.FanCtrl;
	bool hot = this->MaxTemp > this->ManModeExitInternal;

	int want;
	if (t >= target || hot)       want = 64;
	else if (t >= target - 5)     want = 7;
	else                          want = floor;

	if (!hot) {
		if (want != 64 && cur == 64 && t > target - 2)
			want = 64;                                  // hold full speed until 2 C below target
		if (want != 64 && want < 7 && (cur == 7 || cur == 64) && t > target - 7)
			want = 7;                                   // hold level 7 until 2 C below its band
	}
	if (floor == 64 || this->GuardHold())
		want = 64;
	else if (want != 64 && want < floor)
		want = floor;

	if (want == 64 && cur != 64 && cur != 7 && this->fan1speed < 4000)
		want = 7;                                       // spin up regulated first (as Smart)

	if (want != cur)
		this->SetFan("Gorilla", want);
}

//-------------------------------------------------------------------------
//  Compile mode (Gorilla fork, mode 5): for Firefox and kernel builds
//
//  2026-10-02: a 77-minute 12-thread compile ran with the fan pinned at level
//  4 and a build governor reading a chassis zone; the laptop hit its hardware
//  trip. Linux's thermald would have reacted. This mode reacts to the LOAD and
//  to the RATE of rise, not only to the temperature already reached:
//
//    CPU >= target, or MaxTemp > ManModeExit,
//    or the CPU climbed >= 3 C since the last sample   full speed (0x40)
//    CPU >= target - 10, or load >= 60% for 2 samples    level 7
//    load >= 60% now, or CPU >= target - 20              level 5
//    otherwise                                           level 4 (never lower)
//
//  Holds full speed until 3 C below the target and level 7 until 12 C below
//  it while the load stays high, so it does not hunt. Steers by the CPU
//  sensor (sensor 1) like Gorilla mode; the hottest sensor if it does not read.
//-------------------------------------------------------------------------
void FANCONTROL::CompileControl(void) {
	int cpu = this->State.Sensors[0];
	int t = (cpu > 0 && cpu < 128) ? cpu : this->MaxTemp;
	int target = this->CompileTarget;
	int cur = this->State.FanCtrl;
	bool hot = this->MaxTemp > this->ManModeExitInternal;
	bool rising = this->LastCpuTemp > 0 && t - this->LastCpuTemp >= 3;
	bool busy = this->LoadPct >= 60;
	this->LastPrev = this->LastCpuTemp;
	this->LastT = t;
	this->LastCpuTemp = t;
	this->CtlCalls++;
	const char* why;

	// gorilla.5 (crash 2026-10-27 ~17:10): the fan sat at level 4 as a guarded commit started, because the EC
	// reading lags the die. fieldkit guard now says a heavy job runs (guard-hold.txt): full speed for all of it.
	bool guard = this->GuardHold();
	int want;
	if (guard) {
		want = 64; why = "guard"; }
	else if (t >= target || hot || rising) {
		want = 64; why = hot ? "hot" : (t >= target ? "target" : "rise"); }
	else if (t >= target - 10 || this->BusySamples >= 2) {
		want = 7; why = t >= target - 10 ? "near" : "busy2"; }
	else if (busy || t >= target - 20) {
		want = 5; why = busy ? "busy" : "warm"; }
	else {
		want = 4; why = "floor"; }

	// Full speed, once engaged, is held for at least 30 s unless the CPU is far below the target. Without it a
	// spiky load hunted: +5 C -> full, 62 C -> level 7, +8 C -> full ... every 5 s (owner's log, 2026-10-13 01:50).
	unsigned long long now = ::GetTickCount64();
	if (want == 64)
		this->FullHoldUntil = now + 30000;
	else if (now < this->FullHoldUntil && t > target - 20) {
		want = 64; why = "hold30"; }

	if (!hot && !rising && !guard) {
		if (want != 64 && cur == 64 && t > target - 3) {
			want = 64; why = "hold64"; }                // hold full speed until 3 C below target
		if (want != 64 && want < 7 && (cur == 7 || cur == 64) && (t > target - 12 || busy)) {
			want = 7; why = "hold7"; }                  // hold level 7 while hot-ish or still loaded
	}
	if (want != 64 && want < 4)
		want = 4;

	if (want == 64 && cur != 64 && cur != 7 && this->fan1speed < 4000) {
		want = 7; why = "stair"; }                      // spin up regulated first (as Smart)

	// Up at once, down only 30 s after the last raise. A build's load comes in pulses (one job ends, the next
	// starts); stepping down at once made the fan follow every pulse: 4, 5, 7, 4 ... 11 changes in a minute in the
	// bursty test (2026-10-13). Rank: levels 0-7, full speed (64) above them.
	// No exception for a cool CPU: with one (t < target - 30) a bursty load at 36-38 C went 4, 7, 4 every pulse,
	// 9 changes in a minute (2026-10-23). Holding a faster level 30 s more costs noise, never heat.
	auto rank = [](int v) { return v == 64 ? 8 : (v & 0x80 ? -1 : v); };
	if (rank(want) > rank(cur))
		this->LastRaiseTick = now;
	else if (rank(want) < rank(cur) && now - this->LastRaiseTick < 30000) {
		want = cur; why = "holddn"; }
	else if (rank(want) < rank(cur) && rank(cur) >= 0) {
		// gorilla.5: down ONE step at a time, 30 s apart: full -> 7 -> 5 -> 4. Straight from full speed to level 4
		// meant the next pulse of a build started from 2,775 rpm (owner's log, 2026-10-27 17:20: 0x40 -> 0x04)
		int step = cur == 64 ? 7 : (cur >= 6 ? 5 : 4);
		if (rank(step) > rank(want)) {
			want = step; why = "stepdn"; }
		this->LastRaiseTick = now;                      // the next step down waits another 30 s
	}
	this->LastWant = want;
	strcpy_s(this->LastWhy, sizeof(this->LastWhy), why);

	if (want != cur)
		this->SetFan("Compile", want);
}

//-------------------------------------------------------------------------
//  CPU load of the last sample interval, from GetSystemTimes deltas (engine)
//-------------------------------------------------------------------------
void FANCONTROL::SampleLoad(void) {
	FILETIME fi, fk, fu;
	if (!::GetSystemTimes(&fi, &fk, &fu)) return;
	unsigned long long idle = ((unsigned long long)fi.dwHighDateTime << 32) | fi.dwLowDateTime;
	unsigned long long kern = ((unsigned long long)fk.dwHighDateTime << 32) | fk.dwLowDateTime;
	unsigned long long user = ((unsigned long long)fu.dwHighDateTime << 32) | fu.dwLowDateTime;
	if (this->LastKernelT) {
		unsigned long long total = (kern - this->LastKernelT) + (user - this->LastUserT);
		unsigned long long didle = idle - this->LastIdleT;
		if (total > 0)
			this->LoadPct = (int)(100 - (100 * didle) / total);   // kernel time includes idle time
		this->BusySamples = this->LoadPct >= 60 ? this->BusySamples + 1 : 0;
	}
	this->LastIdleT = idle; this->LastKernelT = kern; this->LastUserT = user;
}

//-------------------------------------------------------------------------
//  gorilla.5: %ProgramData%\TPFanControl\guard-hold.txt exists while fieldkit guard runs a heavy job
//  (fieldkit/thermal/brake.py writes it; the last job out, or the machine governor, removes it). A stale file
//  only keeps the fan fast.
//-------------------------------------------------------------------------
bool FANCONTROL::GuardHold(void) {
	char path[MAX_PATH] = "";
	DWORD n = ::GetEnvironmentVariableA("ProgramData", path, sizeof(path));
	if (n == 0 || n >= sizeof(path)) return false;
	strcat_s(path, sizeof(path), "\\TPFanControl\\guard-hold.txt");
	return ::GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

//-------------------------------------------------------------------------
//  gorilla.5, the "Gorilla Fan speed" line (owner, 2026-10-27: "it shows the speed fan in real time so the user
//  can make an informed decision"): rpm, the share of full speed measured for this laptop (FanLevelRpm), and the
//  level the fan control set. The engine reads the rpm alone every second (ReadFanSpeedOnly); a client takes it
//  from the shared state (PullFanSpeed).
//-------------------------------------------------------------------------
void FANCONTROL::ShowFanSpeed(void) {
	char buf[128], lvl[24];
	int fc = this->State.FanCtrl & 0xff;
	if (fc == 0x40) strcpy_s(lvl, sizeof(lvl), "full speed");
	else if (fc & 0x80) strcpy_s(lvl, sizeof(lvl), "BIOS decides");
	else sprintf_s(lvl, sizeof(lvl), "level %d", fc & 0x07);
	int full = 0;
	for (int k = 0; k < 9; k++) if (this->FanLevelRpm[k] > full) full = this->FanLevelRpm[k];
	if (!SingleFan)
		sprintf_s(buf, sizeof(buf), "%d / %d rpm  (%s)", this->fan1speed, this->fan2speed, lvl);
	else if (full > 0)
		sprintf_s(buf, sizeof(buf), "%d rpm  =  %d%% of full  (%s)", this->fan1speed,
			(this->fan1speed * 100 + full / 2) / full, lvl);
	else
		sprintf_s(buf, sizeof(buf), "%d rpm  (%s)", this->fan1speed, lvl);
	::SetDlgItemText(this->hwndDialog, 8102, buf);
}

bool FANCONTROL::ReadFanSpeedOnly(void) {
	if (!this->EcAccess.Lock(50))
		return false;                                   // the full read or another tool has the EC: next second
	char lo = 0, hi = 0, lo2 = 0, hi2 = 0;
	bool ok = this->WriteByteToEC(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN1)
		&& this->ReadByteFromEC(TP_ECOFFSET_FANSPEED, &lo) && this->ReadByteFromEC(TP_ECOFFSET_FANSPEED + 1, &hi)
		&& this->ReadByteFromEC(TP_ECOFFSET_FANSPEED, &lo2) && this->ReadByteFromEC(TP_ECOFFSET_FANSPEED + 1, &hi2);
	this->FreeECAccess();
	int rpm = ((unsigned char)hi << 8) | (unsigned char)lo;
	if (!ok || lo != lo2 || hi != hi2 || rpm > 0x1fff)
		return false;                                   // two equal readings, as ReadEcStatus demands
	this->State.Fan1SpeedLo = lo;
	this->State.Fan1SpeedHi = hi;
	this->fan1speed = rpm;
	FCSHARED* shared = SharedState();
	if (shared) {
		shared->fan1lo = (unsigned char)lo;
		shared->fan1hi = (unsigned char)hi;
	}
	return true;
}

void FANCONTROL::PullFanSpeed(void) {
	FCSHARED* shared = SharedState();
	if (!shared)
		return;
	this->State.Fan1SpeedLo = (char)shared->fan1lo;
	this->State.Fan1SpeedHi = (char)shared->fan1hi;
	this->State.FanCtrl = (char)shared->fanCtrl;
	int rpm = ((shared->fan1hi & 0xff) << 8) | (shared->fan1lo & 0xff);
	if (rpm <= 0x1fff)
		this->fan1speed = rpm;
}

//-------------------------------------------------------------------------
//  %ProgramData%\TPFanControl\status.txt: one line, rewritten atomically
//  every sample by the engine, so a build harness has the EC cpu reading
//  without the window: ts=..;mode=..;cpu=..;max=..;load=..;fan=0x..;rpm=..
//-------------------------------------------------------------------------
void FANCONTROL::WriteStatusFile(void) {
	char dir[MAX_PATH] = "";
	DWORD n = ::GetEnvironmentVariableA("ProgramData", dir, sizeof(dir));
	if (n == 0 || n >= sizeof(dir)) return;
	strcat_s(dir, sizeof(dir), "\\TPFanControl");
	::CreateDirectoryA(dir, NULL);
	char tmp[MAX_PATH], fin[MAX_PATH], line[256];
	sprintf_s(tmp, sizeof(tmp), "%s\\status.txt.tmp", dir);
	sprintf_s(fin, sizeof(fin), "%s\\status.txt", dir);
	int cpu = this->State.Sensors[0];
	// want/why/t/prev/n: the LAST Compile-mode decision (this line is written before this cycle decides)
	sprintf_s(line, sizeof(line), "ts=%lu;mode=%d;cpu=%d;max=%d;load=%d;fan=0x%02x;rpm=%d;want=0x%02x;why=%s;t=%d;prev=%d;n=%u\r\n",
		(unsigned long)::GetTickCount64(), this->CurrentMode, (cpu > 0 && cpu < 128) ? cpu : -1, this->MaxTemp,
		this->LoadPct, this->State.FanCtrl, this->fan1speed, this->LastWant & 0xff, this->CtlCalls ? this->LastWhy : "none",
		this->LastT, this->LastPrev, this->CtlCalls);
	HANDLE h = ::CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return;
	DWORD w = 0;
	::WriteFile(h, line, (DWORD)strlen(line), &w, NULL);
	::CloseHandle(h);
	::MoveFileExA(tmp, fin, MOVEFILE_REPLACE_EXISTING);
}

//-------------------------------------------------------------------------
//  smart fan control depending on temperature
//-------------------------------------------------------------------------
void FANCONTROL::SmartControl(void) {
	char obuf[256] = "";
	int i, newfanctrl = -1, levelIndex = -1, fanctrl = this->State.FanCtrl;

	if (this->PreviousMode == 1) {
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Change Mode from BIOS->");
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart, recalculate fan speed");
		this->Trace(obuf);
	}

	if (this->PreviousMode == 4) {
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Change Mode from Gorilla->");
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart, recalculate fan speed");
		this->Trace(obuf);
		obuf[0] = 0;
	}

	if (this->PreviousMode == 3) {
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Change Mode from Manual->");
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Smart, recalculate fan speed");
		this->Trace(obuf);
	}

	//i         Temp Fan Hup Hdown 
	//0 Level = 50   0   0   0 
	//1 Level = 60   1   0   5 <--- means, when going down switch to this level at 55
	//2 Level = 70   2   0   0 
	//3 Level = 80   4   5   0 <--- means, when going up, switch this level at 85
	//4 Level = 90   7   0   0 
	//5 Level = 95   64  0   0 
	//6 Level = 105 128  0   0 

	if ((fanctrl > 7 && (fanctrl != 64 || !Lev64Norm)) || this->PreviousMode == 3 || this->PreviousMode == 1 || this->PreviousMode == 4) {
		fanctrl = 0;
		levelIndex = 0;
		newfanctrl = 0;
	}

	// Check for fan speed ramp upwards
	for (i = 0; this->SmartLevels[i].temp != -1; i++) {
		if (this->MaxTemp >= this->SmartLevels[i].temp + this->SmartLevels[i].hystUp && this->SmartLevels[i].fan >= fanctrl) {
			newfanctrl = this->SmartLevels[i].fan;
			levelIndex = i;
		}
	}

	// Check for fan speed ramp downwards
	if (newfanctrl == -1) {
		for (i = 0; this->SmartLevels[i].temp != -1; i++) {
			if (this->MaxTemp <= this->SmartLevels[i].temp - this->SmartLevels[i].hystDown && this->SmartLevels[i].fan < fanctrl) {
				newfanctrl = this->SmartLevels[i].fan;
				levelIndex = i;
				break;
			}
		}
	}

	// in disengaged mode the EC crawls up from its current duty, useless in an
	// emergency; run 7 regulated until the fan is spun up, then top out on 64
	// so the crawl only covers the last few hundred rpm
	// Gorilla fork, gorilla.4: "spun up" is 4000 rpm OR one sample at level 7. An L15 Gen 3 tops out
	// at ~3490 rpm, so with 4000 alone full speed never engaged (measured 2026-10-13: 90 C at level 7).
	if (newfanctrl == 64 && fanctrl != 64 && fanctrl != 7 && this->fan1speed < 4000)
		newfanctrl = 7;

	if (newfanctrl != -1 && newfanctrl != this->State.FanCtrl) {
		this->SetFan("Smart", newfanctrl);
	}

	return;
}

//-------------------------------------------------------------------------
//  set fan state via EC
//-------------------------------------------------------------------------
bool FANCONTROL::SetFan(const char* source, int fanctrl, bool final) {
	char obuf[256] = "",
		 obuf2[256],
		 datebuf[128];
	int ok = 0, fan1_ok = 0, fan2_ok = 0;
	char* p = obuf;

	if (g_clientMode)
		return false;

	if (this->FanBeepFreq && this->FanBeepDura)
		::Beep(this->FanBeepFreq, this->FanBeepDura);

	this->CurrentDateTimeLocalized(datebuf, sizeof(datebuf));

	p += sprintf_s(p, sizeof(obuf) - (p - obuf), "%s: Set fan control to 0x%02x, ", source, fanctrl);
	if (this->SmartLevels2[0].temp2 != 0 && strcmp(source, "Smart") == 0) // fix: was pointer compare
		p += sprintf_s(p, sizeof(obuf) - (p - obuf), "Mode %d, ", this->IndSmartLevel == 1 ? 2 : 1);
	p += sprintf_s(p, sizeof(obuf) - (p - obuf), "Result: ");

	if (this->ActiveMode && !this->FinalSeen) {
		if (!this->LockECAccess()) return false;

		for (int i = 0; i < 5; i++) {
			// set new fan1 level
			ok = this->WriteByteToEC(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN1);
			ok = this->WriteByteToEC(TP_ECOFFSET_FAN, fanctrl);
			::Sleep(100);
			// verify completion of fan1
			fan1_ok = this->ReadByteFromEC(TP_ECOFFSET_FAN, &this->State.FanCtrl);
			::Sleep(100);

			if (!SingleFan) {
				// set new fan2 level
				ok = this->WriteByteToEC(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN2);
				ok = this->WriteByteToEC(TP_ECOFFSET_FAN, fanctrl);
				::Sleep(100);
				// verify completion of fan2
				fan2_ok = this->ReadByteFromEC(TP_ECOFFSET_FAN, &this->State.FanCtrl);
				::Sleep(100);
			}
			else {
				fan2_ok = true;
			}

			if (fan1_ok && fan2_ok) {
				p += sprintf_s(p, sizeof(obuf) - (p - obuf), "[i=%d] ", i);
				break;
			}

			::Sleep(250);
		}

		this->FreeECAccess();

		if (this->State.FanCtrl == fanctrl) {
			p += sprintf_s(p, sizeof(obuf) - (p - obuf), "OK");
			ok = true;
			if (final)
				this->FinalSeen = true;
		}
		else {
			p += sprintf_s(p, sizeof(obuf) - (p - obuf), "FAILED!!");
			ok = false;
		}
	}
	else {
		p += sprintf_s(p, sizeof(obuf) - (p - obuf), "IGNORED!(passive mode)");
	}

	sprintf_s(obuf2, sizeof(obuf2), "%s   (%s)", obuf, datebuf);
	::SetDlgItemText(this->hwndDialog, 8113, obuf2);

	this->Trace(this->CurrentStatus);
	this->Trace(obuf);

	if (!final)
		::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);

	return ok;
}

bool FANCONTROL::SetHdw(const char* source, int hdwctrl, int HdwOffset, int AnyWayBit) {
	char obuf[256] = "",
		 obuf2[256],
		 datebuf[128],
		 newhdwctrl;
	int ok = 0;

	if (g_clientMode)
		return false;

	if (!this->LockECAccess()) return false;

	this->CurrentDateTimeLocalized(datebuf, sizeof(datebuf));

	for (int i = 0; i < 5; i++) {
		ok = this->ReadByteFromEC(HdwOffset, &newhdwctrl);
		if (newhdwctrl & hdwctrl) {
			ok = this->WriteByteToEC(HdwOffset, (newhdwctrl - hdwctrl) | AnyWayBit);
			hdwctrl = newhdwctrl - hdwctrl;
		}
		else {
			ok = this->WriteByteToEC(HdwOffset, (newhdwctrl + hdwctrl) | AnyWayBit);
			hdwctrl = newhdwctrl + hdwctrl;
		}

		ok = this->ReadByteFromEC(HdwOffset, &newhdwctrl);

		if (hdwctrl == newhdwctrl)
			break;

		::Sleep(300);
	}

	sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "%s: Set EC register 0x%02x to %d, ", source, HdwOffset, hdwctrl);
	sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "Result: ");

	if (hdwctrl == newhdwctrl) {
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "OK");
		ok = true;
	}
	else {
		sprintf_s(obuf + strlen(obuf), sizeof(obuf) - strlen(obuf), "COULD NOT SET HARDWARE STATE!!!!");
		ok = false;
	}


	// display result
	sprintf_s(obuf2, sizeof(obuf2), "%s   (%s)", obuf, datebuf);

	::SetDlgItemText(this->hwndDialog, 8113, obuf2);

	this->Trace(obuf);

	this->FreeECAccess();

	return ok;
}

//-------------------------------------------------------------------------
//  check two EC status samples for accpetable equivalence
//-------------------------------------------------------------------------
bool FANCONTROL::SampleMatch(FCSTATE* smp1, FCSTATE* smp2) {

	// match for identical fanctrl settings
	if (smp1->FanCtrl != smp2->FanCtrl) return false;

	// insert any further match criteria here:
	// -----------------------
	//
	// if (......) ......
	//
	// -----------------------

	return true;
}

//-------------------------------------------------------------------------
//  lock access to the EC controller
//-------------------------------------------------------------------------
bool FANCONTROL::LockECAccess() {
	const int numTries = 10;
	const int sleepTicks = 100;

	for (int i = 0; i < numTries; i++) {
		if (this->EcAccess.Lock(100)) return TRUE;

		if (i + 1 < numTries) ::Sleep(sleepTicks);
	}

	this->Trace("Could not acquire mutex to read EC status");

	return false;
}

//-------------------------------------------------------------------------
//  relinquisch any lock access to the EC controller
//-------------------------------------------------------------------------
void FANCONTROL::FreeECAccess() {
	this->EcAccess.Unlock();
}

//-------------------------------------------------------------------------
//  read fan and temperatures from embedded controller
//-------------------------------------------------------------------------
bool FANCONTROL::ReadEcStatus(FCSTATE* pfcstate) {
	FCSTATE sample1, sample2;
	const int numTries = 10;
	const int sleepTicks = 200;

	if (pfcstate == NULL) {
		this->Trace("ReadEcStatus: pfcstate is null");
		return false;
	}

	if (!this->LockECAccess()) return false;

	// reading from the EC seems to yield erratic results at times (probably
	// due to collision with other drivers reading from the port).  So try
	// up to ten times to read two samples which look ok and have matching
	// values, using the above match function

	for (int i = 0; i < numTries; i++) {
		const bool okSample1 = this->ReadEcRaw(&sample1);
		const bool okSample2 = this->ReadEcRaw(&sample2);

		if (okSample1 && okSample2 && this->SampleMatch(&sample1, &sample2)) {
			memcpy(pfcstate, &sample2, sizeof(*pfcstate));
			this->FreeECAccess();
			return TRUE;
		}

		if (i + 1 < numTries) ::Sleep(sleepTicks);
	}

	this->FreeECAccess();

	this->Trace("failed to read reliable status values from EC");

	return false;
}

//-------------------------------------------------------------------------
//  read fan and temperatures from embedded controller
//-------------------------------------------------------------------------
bool FANCONTROL::ReadEcRaw(FCSTATE* pfcstate) {

	// At any point in time, a failure in "ReadByteFromEC" or "WriteByteToEC"
	// is a reason to abort the entire process and return "false" to indicate failure.
	// This process will be retried by the caller of this routine.

	pfcstate->FanCtrl = -1;

	// Status Register
	if (!ReadByteFromEC(TP_ECOFFSET_FAN, &pfcstate->FanCtrl)) {
		this->Trace("failed to read status register from EC");
		return false;
	}

	//
	// Fan 1 first
	//

	// Select 
	if (!WriteByteToEC(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN1)) {
		this->Trace("failed to select Fan 1 in EC");
		return false;
	}

	// Lo
	if (!ReadByteFromEC(TP_ECOFFSET_FANSPEED, &pfcstate->Fan1SpeedLo)) {
		this->Trace("failed to read FanSpeedLowByte 1 from EC");
		return false;
	}

	// Hi
	if (!ReadByteFromEC(TP_ECOFFSET_FANSPEED + 1, &pfcstate->Fan1SpeedHi)) {
		this->Trace("failed to read FanSpeedHighByte 1 from EC");
		return false;
	}

	if (!SingleFan) {
		//
		// Fan 2 last
		//
		if (!WriteByteToEC(TP_ECOFFSET_FAN_SWITCH, TP_ECVALUE_SELFAN2)) {
			this->Trace("failed to select Fan 2 in EC");
			return false;
		}

		// Lo
		if (!ReadByteFromEC(TP_ECOFFSET_FANSPEED, &pfcstate->Fan2SpeedLo)) {
			this->Trace("failed to read FanSpeedLowByte 2 from EC");
			return false;
		}

		// Hi
		if (!ReadByteFromEC(TP_ECOFFSET_FANSPEED + 1, &pfcstate->Fan2SpeedHi)) {
			this->Trace("failed to read FanSpeedHighByte 2 from EC");
			return false;
		}
	}
	else {
		pfcstate->Fan2SpeedLo = pfcstate->Fan1SpeedLo;
		pfcstate->Fan2SpeedHi = pfcstate->Fan1SpeedHi;
	}

	// Get Sensors finally

	int i, idxtemp, ok = TRUE;

	memset(pfcstate->Sensors, 0, sizeof(pfcstate->Sensors));

	if (!this->UseTWR) {

		idxtemp = 0;

		for (i = 0; i < 8; i++) {    // temp sensors 0x78 - 0x7f
			pfcstate->SensorAddr[idxtemp] = TP_ECOFFSET_TEMP0 + i;

			pfcstate->SensorName[idxtemp] = this->gSensorNames[idxtemp];

			if (ReadByteFromEC(TP_ECOFFSET_TEMP0 + i, &pfcstate->Sensors[idxtemp])) {
				if (this->ShowBiasedTemps)
					pfcstate->Sensors[idxtemp] = pfcstate->Sensors[idxtemp] - this->SensorOffset[idxtemp].offs;
			}
			else {
				this->Trace("failed to read a TEMP0 byte from EC");
				return false;
			}

			idxtemp++;
		}

		for (i = 0; i < 4; i++) {    // temp sensors 0xC0 - 0xC4
			pfcstate->SensorAddr[idxtemp] = TP_ECOFFSET_TEMP1 + i;

			pfcstate->SensorName[idxtemp] = "n/a";

			if (!this->NoExtSensor) {
				pfcstate->SensorName[idxtemp] = this->gSensorNames[idxtemp];

				if (ReadByteFromEC(TP_ECOFFSET_TEMP1 + i, &pfcstate->Sensors[idxtemp])) {
					if (this->ShowBiasedTemps)
						pfcstate->Sensors[idxtemp] = pfcstate->Sensors[idxtemp] - this->SensorOffset[idxtemp].offs;
				}
				else {
					this->Trace("failed to read a TEMP1 byte from EC");
					return false;
				}
			}

			idxtemp++;
		}
	}
	else {
		// the TWR mailbox lives outside the ports the PawnIO module exposes
		if (!PortAccess_AllowsPort(0x1610)) {
			this->Trace("UseTWR needs a port backend that reaches 0x1600-0x161f, use TVicPort or disable UseTWR");
			return false;
		}

		char data = -1;
		char dataOut[16] = { };
		int iOK = false;
		int iTimeout = 100;
		int iTimeoutBuf = 1000;
		int iTime = 0;
		int iTick = 10;
		int iNumTry = 0;

	retry:
		iNumTry++;

		if (iNumTry >= 3) {
			this->Trace("failed to read temps , EC is not ready for TWR");
			return false;
		}

		for (iTime = 0; iTime < iTimeoutBuf; iTime += iTick) {    // wait for ec ready
			data = (char)PortAccess_ReadByte(0x1604) & 0xff;                // or timeout iTimeoutBuf = 1000
			if (!data)                                            // ec is ready: ctrlprt = 0
				break;
			if (data & 0x50)                                    // some unrequested outputis waiting
				PortAccess_ReadByte(0x161f);                                // clear data output
			::Sleep(iTick);
		}

		PortAccess_WriteByte(0x1610, 0x20);                            // tell them we want to read
		data = (char)PortAccess_ReadByte(0x1604) & 0xff;
		if (!(data & 0x20))                                    // ec is not ready
			goto retry;

		for (int i = 1; i < 15; i++) {
			PortAccess_WriteByte(0x1610 + i, 0x00);
		}

		PortAccess_WriteByte(0x161f, 0x00);

		for (iTime = 0; iTime < iTimeoutBuf; iTime++) {            // wait for full buffers to clear
			data = (char)PortAccess_ReadByte(0x1604) & 0xff;                // or timeout iTimeoutBuf = 1000
			if (data == 0x50)
				break;
		}

		if (data != 0x50)
			goto retry;

		for (int i = 0; i < 16; i++) {
			dataOut[i] = (char)PortAccess_ReadByte(0x1610 + i) & 0xff;
		}

		pfcstate->SensorAddr[0] = 0x78;
		pfcstate->SensorName[0] = this->gSensorNames[0];
		pfcstate->Sensors[0] = dataOut[0];

		pfcstate->SensorAddr[1] = 0x79;
		pfcstate->SensorName[1] = this->gSensorNames[1];
		pfcstate->Sensors[1] = dataOut[1];

		pfcstate->SensorAddr[2] = 0x7a;
		pfcstate->SensorName[2] = this->gSensorNames[2];
		pfcstate->Sensors[2] = dataOut[2];

		pfcstate->SensorAddr[3] = 0x7b;
		pfcstate->SensorName[3] = this->gSensorNames[3];
		pfcstate->Sensors[3] = dataOut[3];

		pfcstate->SensorAddr[4] = 0x7c;
		pfcstate->SensorName[4] = this->gSensorNames[4];
		pfcstate->Sensors[4] = dataOut[4];

		pfcstate->SensorAddr[5] = 0x7d;
		pfcstate->SensorName[5] = this->gSensorNames[5];
		pfcstate->Sensors[5] = dataOut[6];

		pfcstate->SensorAddr[6] = 0x7e;
		pfcstate->SensorName[6] = this->gSensorNames[6];
		pfcstate->Sensors[6] = dataOut[8];

		pfcstate->SensorAddr[7] = 0x7f;
		pfcstate->SensorName[7] = this->gSensorNames[7];
		pfcstate->Sensors[7] = dataOut[9];

		pfcstate->SensorAddr[8] = 0xc0;
		pfcstate->SensorName[8] = this->gSensorNames[8];
		pfcstate->Sensors[8] = dataOut[10];

		pfcstate->SensorAddr[9] = 0xc1;
		pfcstate->SensorName[9] = this->gSensorNames[9];
		pfcstate->Sensors[9] = dataOut[11];

		pfcstate->SensorAddr[10] = 0xc2;
		pfcstate->SensorName[10] = this->gSensorNames[10];
		pfcstate->Sensors[10] = dataOut[12];

		pfcstate->SensorAddr[11] = 0xc3;
		pfcstate->SensorName[11] = this->gSensorNames[11];
		pfcstate->Sensors[11] = dataOut[13];
	}

	return ok;
}
