#pragma once
#include <cstdint>

// Bus supplies read/write of a single port, a monotonic clock and a short pause.
// No read of the data port is made until the EC advertises a requested reply.
template<class Bus>
bool TransferEcByte(Bus& bus, uint16_t ctrl, uint16_t data, uint8_t address,
                    uint8_t& value, bool write, uint64_t deadline) {
	constexpr uint8_t inputFull = 2, outputFull = 1;
	auto wait = [&](uint8_t mask, uint8_t expected) {
		while (bus.now() < deadline) {
			uint8_t status;
			if (!bus.read(ctrl, status)) return false;
			if ((status & mask) == expected) return true;
			bus.pause();
		}
		return false;
	};
	if (!wait(inputFull | outputFull, 0) || !bus.write(ctrl, write ? 0x81 : 0x80)
		|| !wait(inputFull, 0) || !bus.write(data, address) || !wait(inputFull, 0))
		return false;
	if (write) return bus.write(data, value) && wait(inputFull, 0);
	return wait(outputFull, outputFull) && bus.read(data, value);
}
