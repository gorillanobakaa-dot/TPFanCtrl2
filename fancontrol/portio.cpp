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

// Docs:
// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/12_ACPI_Embedded_Controller_Interface_Specification/embedded-controller-command-set.html

#include "_prec.h"
#include "fancontrol.h"
#include "portaccess.h"
#include "ectransaction.h"

// Registers of the embedded controller
//
// Note: Testing on a Lenovo P53 with EC at 0x1600/0x1604 and a Lenovo T430 with EC at 0x62/0x66 suggests that the EC command set is
//       consistent across these two models, but the port addresses differ.  This code attempts to auto-detect which port layout is 
//       present by trying one, then the other if the first fails.
//
// Note: Testing has established that the embedded controller on the laptop models of interest do not support burst mode, 
//		 so the burst mode command and status bit are not implemented in this code.  If a future model is found that does support burst
//       mode, then the burst enable/disable commands can be issued as needed and the burst status bit can be checked to confirm that 
//       the controller is in burst mode before attempting to read/write multiple bytes in a row. See the ACPI Embedded Controller Interface
//       Specification (link above) for details on burst mode.
// 
// From TPFanControl V0.6.3+ V.2.2.0+
constexpr auto ACPI_EC_TYPE1_CTRLPORT = 0x1604;
constexpr auto ACPI_EC_TYPE1_DATAPORT = 0x1600;
// From TPFanControl V0.6.2 final
constexpr auto ACPI_EC_TYPE2_CTRLPORT = 0x66;
constexpr auto ACPI_EC_TYPE2_DATAPORT = 0x62;

struct EcPortLayout {
	int ctrl;
	int data;
	const char* name;
};

constexpr EcPortLayout EC_PORT_LAYOUTS[] = {
	{ ACPI_EC_TYPE1_CTRLPORT, ACPI_EC_TYPE1_DATAPORT, "ACPI_EC_TYPE1" },
	{ ACPI_EC_TYPE2_CTRLPORT, ACPI_EC_TYPE2_DATAPORT, "ACPI_EC_TYPE2" },
};
constexpr size_t EC_PORT_LAYOUT_COUNT = sizeof(EC_PORT_LAYOUTS) / sizeof(EC_PORT_LAYOUTS[0]);

constexpr int DEFAULT_SLEEP_TICKS = 10;
constexpr int DEFAULT_TIMEOUT_MS = 1000;

constexpr int TRACE_BUFFER_SIZE = 160;


//-------------------------------------------------------------------------
// initialize embedded controller ports if not yet selected
//--------------------------------------------------------------------------
static void
InitializeEcPorts(int& ctrlPort, int& dataPort) {
	if (ctrlPort != 0 && dataPort != 0) return;

	// first layout the backend can reach; PawnIO only covers 0x62/0x66
	for (const auto& layout : EC_PORT_LAYOUTS) {
		if (!PortAccess_AllowsPort(static_cast<USHORT>(layout.ctrl))) continue;

		ctrlPort = layout.ctrl;
		dataPort = layout.data;
		return;
	}

	ctrlPort = EC_PORT_LAYOUTS[0].ctrl;
	dataPort = EC_PORT_LAYOUTS[0].data;
}

//--------------------------------------------------------------------------
// resolve a human-readable EC layout name for tracing
//--------------------------------------------------------------------------
static const char*
GetEcLayoutName(int ctrlPort, int dataPort) {
	for (const auto& layout : EC_PORT_LAYOUTS) {
		if (layout.ctrl == ctrlPort && layout.data == dataPort) {
			return layout.name;
		}
	}
	return "ACPI_EC_UNKNOWN";
}

//--------------------------------------------------------------------------
// build ordered EC layout attempts: current first, then remaining known ones
//--------------------------------------------------------------------------
static size_t
BuildEcPortAttempts(
	FANCONTROL* pThis,
	EcPortLayout* attempts,
	size_t maxAttempts,
	char* traceText,
	size_t traceSize) {
	size_t attemptCount = 0;

	if (pThis->EC_CTRL == 0 || pThis->EC_DATA == 0) {
		InitializeEcPorts(pThis->EC_CTRL, pThis->EC_DATA);
		sprintf_s(traceText, traceSize,
			"Trying %s (ctrl=0x%04X data=0x%04X) first",
			GetEcLayoutName(pThis->EC_CTRL, pThis->EC_DATA), pThis->EC_CTRL, pThis->EC_DATA);
		pThis->Trace(traceText);
	}

	// First try currently selected ports
	if (attemptCount < maxAttempts && PortAccess_AllowsPort(static_cast<USHORT>(pThis->EC_CTRL))) {
		attempts[attemptCount++] = {
			pThis->EC_CTRL,
			pThis->EC_DATA,
			GetEcLayoutName(pThis->EC_CTRL, pThis->EC_DATA)
		};
	}

	// Then try the other known layouts the backend can reach
	for (const auto& layout : EC_PORT_LAYOUTS) {
		if (layout.ctrl == pThis->EC_CTRL && layout.data == pThis->EC_DATA) continue;
		if (!PortAccess_AllowsPort(static_cast<USHORT>(layout.ctrl))) continue;
		if (attemptCount >= maxAttempts) break;
		attempts[attemptCount++] = layout;
	}

	return attemptCount;
}

//--------------------------------------------------------------------------
struct SystemEcPorts {
	bool read(USHORT port, UCHAR& value) { return PortAccess_ReadByte(port, value); }
	bool write(USHORT port, UCHAR value) { return PortAccess_WriteByte(port, value); }
	ULONGLONG now() { return GetTickCount64(); }
	void pause() { Sleep(DEFAULT_SLEEP_TICKS); }
};

//-------------------------------------------------------------------------
// read a byte from the embedded controller (EC) via port io 
//-------------------------------------------------------------------------
bool
FANCONTROL::ReadByteFromEC(int offset, char* pdata) {
	char traceText[TRACE_BUFFER_SIZE] = "";

	const UCHAR ecOffset = static_cast<UCHAR>(offset);

	EcPortLayout attempts[EC_PORT_LAYOUT_COUNT + 1];
	const size_t attemptCount = BuildEcPortAttempts(this, attempts, ARRAYMAX(attempts), traceText, sizeof(traceText));

	for (size_t i = 0; i < attemptCount; i++) {
		const auto& layout = attempts[i];

		SystemEcPorts bus;
		UCHAR value = 0;
		if (TransferEcByte(bus, layout.ctrl, layout.data, ecOffset, value, false,
			bus.now() + DEFAULT_TIMEOUT_MS)) {
			*pdata = static_cast<char>(value);
			if (i > 0) {
				sprintf_s(traceText, sizeof(traceText),
					"readec: SUCCESS after layout switch to %s (ctrl=0x%04X data=0x%04X)",
					layout.name, layout.ctrl, layout.data);
				this->Trace(traceText);
				this->EC_CTRL = layout.ctrl;
				this->EC_DATA = layout.data;
			}
			return true;
		}

		if (i + 1 < attemptCount) {
			const auto& nextLayout = attempts[i + 1];
			sprintf_s(traceText, sizeof(traceText),
				"readec: switching to %s (ctrl=0x%04X data=0x%04X)",
				nextLayout.name, nextLayout.ctrl, nextLayout.data);
			this->Trace(traceText);
		}
	}

	this->Trace("readec: FAILED - all ACPI_EC port layouts exhausted");
	return false;
}

//-------------------------------------------------------------------------
// write a byte to the embedded controller (EC) via port io
//-------------------------------------------------------------------------
bool
FANCONTROL::WriteByteToEC(int offset, char NewData) {
	const UCHAR ecOffset = static_cast<UCHAR>(offset);
	const UCHAR ecData = static_cast<UCHAR>(NewData);

	// Reads establish a port layout. Never replay a failed write on another layout.
	if (!EC_CTRL || !EC_DATA || !PortAccess_AllowsPort(static_cast<USHORT>(EC_CTRL)))
		return false;
	SystemEcPorts bus;
	UCHAR value = ecData;
	return TransferEcByte(bus, EC_CTRL, EC_DATA, ecOffset, value, true, bus.now() + DEFAULT_TIMEOUT_MS);
}
