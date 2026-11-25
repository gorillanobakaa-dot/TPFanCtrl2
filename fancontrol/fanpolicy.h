#pragma once

inline bool FanReadyForFull(int current, int rpm, int level7Rpm) {
	if (current == 64) return true;
	if (rpm <= 0 || rpm > 0x1fff) return false;
	if (rpm >= 4000) return true;
	if (current != 7) return false;
	return level7Rpm > 0 ? rpm * 100LL >= level7Rpm * 85LL : true;
}

inline int GorillaFanLevel(int temp, int target, int floor, int current, bool hot, bool guard) {
	if (hot || guard || floor == 64 || temp >= target) return 64;
	if (current == 64 && temp > target - 2) return 64;
	if (temp >= target - 5 || ((current == 7 || current == 64) && temp > target - 7))
		return 7;
	return floor;
}

struct CompileDemand {
	int level;
	const char* reason;
};

inline CompileDemand CompileFanDemand(int temp, int target, bool hot, bool rising,
                                      bool guard, bool busy, int busySamples) {
	if (guard) return {64, "guard"};
	if (hot) return {64, "hot"};
	if (temp >= target) return {64, "target"};
	if (rising) return {64, "rise"};
	if (temp >= target - 10) return {7, "near"};
	if (busySamples >= 2) return {7, "busy2"};
	if (busy) return {5, "busy"};
	if (temp >= target - 20) return {5, "warm"};
	return {4, "floor"};
}
