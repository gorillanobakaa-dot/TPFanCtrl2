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
#include "taskbartexticon.h"
#include "sharedstate.h"
#include "portaccess.h"
#include <vector>
#include <string>
#include <winevt.h>
#include <shlobj.h>
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

extern bool g_clientMode;
extern HANDLE CreateSharedEvent(const char* name);
static void InitCustomTitleBar(HWND dlg);   // custom title bar, defined below

DEFINE_GUID(GUID_LIDSWITCH_STATE_CHANGE,
	0xba3e0f4d, 0xb817, 0x4094,
	0xa2, 0xd1, 0xd5, 0x63, 0x79, 0xe6, 0xa0, 0xf3);

//-------------------------------------------------------------------------
//  constructor
//-------------------------------------------------------------------------
FANCONTROL::FANCONTROL(HINSTANCE hinstapp)
	: 
	hinstapp(hinstapp),
	m_hinstapp(hinstapp),
	hwndDialog(NULL),
	hEventSubscription(NULL),
	CurrentMode(-1),
	PreviousMode(-1),
	Cycle(5),
	IconCycle(1),
	ReIcCycle(0),
	NoExtSensor(0),
	FanSpeedLowByte(0x84),
	CurrentIcon(-1),
	hThread(NULL),
	FanBeepFreq(440),
	FanBeepDura(50),
	ReadErrorCount(0),
	MaxReadErrors(10),
	NoBallons(0),
	HK_BIOS_Method(0),
	HK_BIOS(0),
	HK_Manual_Method(0),
	HK_Manual(0),
	HK_Smart_Method(0),
	HK_Smart(0),
	HK_SM1_Method(0),
	HK_SM1(0),
	HK_SM2_Method(0),
	HK_SM2(0),
	HK_TG_BS_Method(0),
	HK_TG_BS(0),
	HK_TG_BM_Method(0),
	HK_TG_BM(0),
	HK_TG_MS_Method(0),
	HK_TG_MS(0),
	HK_TG_12_Method(0),
	HK_TG_12(0),
	EC_DATA(0),
	EC_CTRL(0),
	BluetoothEDR(0),
	ManModeExit(80),
	ManModeExitInternal(80),
	ShowBiasedTemps(0),
	SecWinUptime(0),
	SecStartDelay(0),
	SlimDialog(0),
	Log2File(0),
	StayOnTop(0),
	Log2csv(0),
	ShowAll(0),
	ShowTempIcon(1),
	pTaskbarIcon(NULL),
	Fahrenheit(FALSE),
	MinimizeToSysTray(TRUE),
	IconColorFan(FALSE),
	Lev64Norm(FALSE),
	StartMinimized(FALSE),
	CaptionHoverColor(RGB(0x00, 0xB4, 0xFF)),   // bright azure (Gorilla fork)
	NoWaitMessage(TRUE),
	MinimizeOnClose(TRUE),
	Runs_as_service(FALSE),
	ActiveMode(false),
	ManFanSpeed(7),
	UseTWR(0),
	FinalSeen(false),
	SingleFan(0),
	PowerSuspendMode(1), // 0 Disable, 1 BIOS(default), 2 Auto, 3 Manual(OFF), 4 Manual on any suspend
	ModernS0Mode(0),  // 0 Disable(default), 1 BIOS, 2 Auto, 3 Manual(OFF)
	savedMode(-1),
	isPowerSuspendState(false),
	isModernS0State(false),
	isLidClosed(false),
	m_fanTimer(NULL),
	m_titleTimer(NULL),
	m_iconTimer(NULL),
	m_renewTimer(NULL),
	m_needClose(false),
	LastCmdSeq(0),
	LastStateSeq(0),
	LastTraceSeq(0),
	savedSmartLevel(-1),
	ppTbTextIcon(NULL),
	pTextIconMutex(new MUTEXSEM(0, "Global\\TPFanControl_ppTbTextIcon")) {

	InitSensorNames();
	InitSmartLevels();
	this->ReadConfig("TPFanControl.ini");
	InitDialogWindow();
	HandleStartupDelay();
	SubscribePowerEvents();
	SetupTaskbarAndTimers();
}

//-------------------------------------------------------------------------
//  initialize sensor names, titles, and icon levels
//-------------------------------------------------------------------------
void FANCONTROL::InitSensorNames() {
	// SensorNames
	// 78-7F (state index 0-7)
	strcpy_s(this->gSensorNames[0], sizeof(this->gSensorNames[0]), "cpu"); // main processor
	strcpy_s(this->gSensorNames[1], sizeof(this->gSensorNames[1]), "aps"); // harddisk protection gyroscope
	strcpy_s(this->gSensorNames[2], sizeof(this->gSensorNames[2]), "crd"); // under PCMCIA slot (front left)
	strcpy_s(this->gSensorNames[3], sizeof(this->gSensorNames[3]), "gpu"); // graphical processor
	strcpy_s(this->gSensorNames[4], sizeof(this->gSensorNames[4]), "bat"); // inside T43 battery
	strcpy_s(this->gSensorNames[5], sizeof(this->gSensorNames[5]), "x7d"); // usually n/a
	strcpy_s(this->gSensorNames[6], sizeof(this->gSensorNames[6]), "bat"); // inside T43 battery
	strcpy_s(this->gSensorNames[7], sizeof(this->gSensorNames[7]), "x7f"); // usually n/a
	// C0-C4 (state index 8-11)
	strcpy_s(this->gSensorNames[8], sizeof(this->gSensorNames[8]), "bus"); // unknown
	strcpy_s(this->gSensorNames[9], sizeof(this->gSensorNames[9]), "pci"); // mini-pci, WLAN, southbridge area
	strcpy_s(this->gSensorNames[10], sizeof(this->gSensorNames[10]), "pwr"); // power supply (get's hot while charging battery)
	strcpy_s(this->gSensorNames[11], sizeof(this->gSensorNames[11]), "xc3"); // usually n/a
	// future
	for (int i = 12; i <= 16; i++)
		strcpy_s(this->gSensorNames[i], sizeof(this->gSensorNames[i]), "");

	// clear title strings
	setzero(this->Title, sizeof(this->Title));
	setzero(this->Title2, sizeof(this->Title2));
	setzero(this->LastTitle, sizeof(this->LastTitle));
	setzero(this->CurrentStatus, sizeof(this->CurrentStatus));
	setzero(this->CurrentStatuscsv, sizeof(this->CurrentStatuscsv));
	setzero(this->IgnoreSensors, sizeof(this->IgnoreSensors));

	this->IconLevels[0] = 50;    // yellow icon level
	this->IconLevels[1] = 55;    // orange icon level
	this->IconLevels[2] = 60;    // red icon level
}

//-------------------------------------------------------------------------
//  initialize smart fan control tables
//-------------------------------------------------------------------------
void FANCONTROL::InitSmartLevels() {
	const int defaultTemps[] = { 50, 55, 60, 65, 70, -1 };
	const int defaultFans[]  = {  0,  3,  5,  7, 128, 0 };
	const int numEntries = 6;

	setzero(this->SmartLevels, sizeof(this->SmartLevels));
	setzero(this->SmartLevels1, sizeof(this->SmartLevels1));
	setzero(this->SmartLevels2, sizeof(this->SmartLevels2));

	for (int i = 0; i < numEntries; i++) {
		this->SmartLevels[i].temp  = defaultTemps[i];
		this->SmartLevels[i].fan   = defaultFans[i];
		this->SmartLevels1[i].temp1 = defaultTemps[i];
		this->SmartLevels1[i].fan1  = defaultFans[i];
	}

	// SmartLevels2 uses 0 instead of 50 for the first temp entry
	for (int i = 0; i < numEntries; i++) {
		this->SmartLevels2[i].temp2 = (i == 0) ? 0 : defaultTemps[i];
		this->SmartLevels2[i].fan2  = defaultFans[i];
	}
}

//-------------------------------------------------------------------------
//  set up dialog window and controls
//-------------------------------------------------------------------------
void FANCONTROL::InitDialogWindow() {
	if (!this->hwndDialog)
		return;

	char buf[256] = "";

	::GetWindowText(this->hwndDialog, this->Title, sizeof(this->Title));
	strcat_s(this->Title, sizeof(this->Title), " V");
	if (SingleFan)
		strcat_s(this->Title, sizeof(this->Title), FANCONTROLVERSIONS);
	else
		strcat_s(this->Title, sizeof(this->Title), FANCONTROLVERSIOND);

	::SetWindowText(this->hwndDialog, this->Title);
	::SetDlgItemText(this->hwndDialog, 8120, this->Title);   // custom title strip
	InitCustomTitleBar(this->hwndDialog);

	::SetWindowLongPtr(this->hwndDialog, GWLP_USERDATA, (LONG_PTR)this);

	::SendDlgItemMessage(this->hwndDialog, 8112, EM_LIMITTEXT, 256, 0);
	::SendDlgItemMessage(this->hwndDialog, 9200, EM_LIMITTEXT, 4096, 0);

		// Init temperature ListView columns (normal mode only)
		{
			HWND hLV = ::GetDlgItem(this->hwndDialog, 8101);
			if (hLV) {
				ListView_SetExtendedListViewStyle(hLV, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
				LVCOLUMNA lvc = {0};
				lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
				lvc.fmt = LVCFMT_LEFT;
				// Column widths share out the list's real width (Gorilla fork):
				// fixed pixel widths were wider than the 70-unit box, and did not
				// grow with display scaling, so text was cut off at 125-150 %.
				RECT rc; ::GetClientRect(hLV, &rc);
				int cw = rc.right - rc.left;
				if (cw < 120) cw = 168;               // not laid out yet: old total
				int w0 = cw * 14 / 100, w1 = cw * 30 / 100, w2 = cw * 30 / 100;
				lvc.cx = w0; lvc.pszText = (LPSTR)"#"; ListView_InsertColumn(hLV, 0, &lvc);
				lvc.cx = w1; lvc.pszText = (LPSTR)"Name"; ListView_InsertColumn(hLV, 1, &lvc);
				lvc.cx = w2; lvc.pszText = (LPSTR)"Temp"; ListView_InsertColumn(hLV, 2, &lvc);
				lvc.cx = cw - w0 - w1 - w2; lvc.pszText = (LPSTR)"EC"; ListView_InsertColumn(hLV, 3, &lvc);
			}
		}

		// Init manual mode ComboBox (editable, normal mode only, supports 0-255)
		{
			HWND hCB = ::GetDlgItem(this->hwndDialog, 8310);
			if (hCB) {
				SendMessageA(hCB, CB_ADDSTRING, 0, (LPARAM)"0");
				SendMessageA(hCB, CB_ADDSTRING, 0, (LPARAM)"1");
				SendMessageA(hCB, CB_ADDSTRING, 0, (LPARAM)"2");
				SendMessageA(hCB, CB_ADDSTRING, 0, (LPARAM)"3");
				SendMessageA(hCB, CB_ADDSTRING, 0, (LPARAM)"4");
				SendMessageA(hCB, CB_ADDSTRING, 0, (LPARAM)"5");
				SendMessageA(hCB, CB_ADDSTRING, 0, (LPARAM)"6");
				SendMessageA(hCB, CB_ADDSTRING, 0, (LPARAM)"7");
				SendMessageA(hCB, CB_ADDSTRING, 0, (LPARAM)"0x40 (max)");
				SendMessageA(hCB, CB_ADDSTRING, 0, (LPARAM)"0x80 (BIOS)");
				char mbuf[16];
				_itoa_s(this->ManFanSpeed, mbuf, 10);
				SetWindowTextA(hCB, mbuf);
			}
		}

	if (SlimDialog == 1) {
		// Fix: destroy the old dialog before replacing
		if (this->hwndDialog)
			::DestroyWindow(this->hwndDialog);
		if (this->StayOnTop)
			this->hwndDialog = ::CreateDialogParam(hinstapp,
				MAKEINTRESOURCE(9001),
				HWND_DESKTOP,
				(DLGPROC)BaseDlgProc,
				(LPARAM)
				this);
		else
			this->hwndDialog = ::CreateDialogParam(hinstapp,
				MAKEINTRESOURCE(9003),
				HWND_DESKTOP,
				(DLGPROC)BaseDlgProc,
				(LPARAM)
				this);
	}
}

//-------------------------------------------------------------------------
//  perform startup delay if needed
//-------------------------------------------------------------------------
void FANCONTROL::HandleStartupDelay() {
	// If the windows fast start feature is turned on, the system uptime
	// WILL NOT BE RESET ON POWER ON, thus making this logic fail. Turn this
	// feature off to make this work.
	DWORD tickCount = GetTickCount();

	char bufsec[1024] = "";

	sprintf_s(bufsec, sizeof(bufsec), "Windows uptime since boot %d sec., SecWinUptime= %d sec.", tickCount / 1000, SecWinUptime);
	this->Trace(bufsec);

	// the port backend opened before tracing existed, drain its messages
	if (!g_clientMode) {
		for (int i = 0; i < PortAccess_InitMessageCount(); i++)
			this->Trace(PortAccess_InitMessage(i));
	}

	if ((SecStartDelay > 0) && ((tickCount / 1000) <= (DWORD)SecWinUptime)) {
		sprintf_s(bufsec, sizeof(bufsec), "Delay startup to allow windows to settle, SecStartDelay= %d sec.", SecStartDelay);
		this->Trace(bufsec);

		if (!NoWaitMessage) {
			sprintf_s(bufsec, sizeof(bufsec),
				"TPFanControl delayed %d sec. after\nboot time (SecWinUptime= %d sec.)\n\nto prevent missing systray icons\nand communication errors between\nTPFanControl and embedded controller\n\n\nTo avoid this message box set\nNoWaitMessage=1 in TPFanControl.ini",
				SecStartDelay, SecWinUptime);

			// Don't show message box when as service in Vista
			OSVERSIONINFOEX os = { sizeof(os) };
			VerifyVersionInfoA(&os, VER_MAJORVERSION, 1);
			if (os.dwMajorVersion >= 6 && Runs_as_service == TRUE)
				;
			else
				MessageBox(NULL, bufsec, "TPFanControl delaying startup", MB_ICONEXCLAMATION);
		}

		// sleep until start time + delay time
		while ((DWORD)(tickCount + SecStartDelay * 1000) >= GetTickCount())
			Sleep(200);
	}
}

//-------------------------------------------------------------------------
//  subscribe to power and modern standby events
//-------------------------------------------------------------------------
void FANCONTROL::SubscribePowerEvents() {
	if (this->PowerSuspendMode) {
		this->hPowerNotify = RegisterPowerSettingNotification(this->hwndDialog, &GUID_LIDSWITCH_STATE_CHANGE, DEVICE_NOTIFY_WINDOW_HANDLE);
		if (this->hPowerNotify == NULL) {
			char errBuf[128];
			sprintf_s(errBuf, sizeof(errBuf), "Failed to subscribe to PowerSetting events, error: %lu", GetLastError());
			this->Trace(errBuf);
		}
		else {
			this->Trace("Subscribed to PowerSetting events");
		}
	}

	if (this->ModernS0Mode) {
		// Subscribe to Modern Standby events (EventId 506 = Entry, 507 = Exit)
		const wchar_t* query = L"*[System[Provider[@Name='Microsoft-Windows-Kernel-Power'] and (EventID=506 or EventID=507)]]";
		this->hEventSubscription = EvtSubscribe(
			NULL,                           // Session (local)
			NULL,                           // SignalEvent
			L"System",                      // Channel path
			query,                          // Query
			NULL,                           // Bookmark
			this,                           // Context (pass FANCONTROL pointer)
			(EVT_SUBSCRIBE_CALLBACK)FANCONTROL::EventLogCallback,
			EvtSubscribeToFutureEvents      // Flags
		);

		if (this->hEventSubscription == NULL) {
			char errBuf[128];
			sprintf_s(errBuf, sizeof(errBuf), "Failed to subscribe to ModernS0 events, error: %lu", GetLastError());
			this->Trace(errBuf);
		}
		else {
			this->Trace("Subscribed to ModernS0 events");
		}
	}
}

//-------------------------------------------------------------------------
//  set up taskbar icon, hotkeys, timers, and initial window state
//-------------------------------------------------------------------------
void FANCONTROL::SetupTaskbarAndTimers() {
	// taskbar icon
	if (this->MinimizeToSysTray) {
		if (this->ShowTempIcon)
			this->pTaskbarIcon = NULL;
		else
			this->pTaskbarIcon = new TASKBARICON(this->hwndDialog, 10, "TPFanControl");
	}

	// read current fan control status and set mode buttons accordingly
	this->CurrentMode = this->ActiveMode;
	this->ModeToDialog(this->CurrentMode);
	this->PreviousMode = 1;

	if (HK_BIOS_Method) RegisterHotKey(this->hwndDialog, 1, HK_BIOS_Method, HK_BIOS);
	if (HK_Smart_Method) RegisterHotKey(this->hwndDialog, 2, HK_Smart_Method, HK_Smart);
	if (HK_Manual_Method) RegisterHotKey(this->hwndDialog, 3, HK_Manual_Method, HK_Manual);
	if (HK_SM1_Method) RegisterHotKey(this->hwndDialog, 4, HK_SM1_Method, HK_SM1);
	if (HK_SM2_Method) RegisterHotKey(this->hwndDialog, 5, HK_SM2_Method, HK_SM2);
	if (HK_TG_BS_Method) RegisterHotKey(this->hwndDialog, 6, HK_TG_BS_Method, HK_TG_BS);
	if (HK_TG_BM_Method) RegisterHotKey(this->hwndDialog, 7, HK_TG_BM_Method, HK_TG_BM);
	if (HK_TG_MS_Method) RegisterHotKey(this->hwndDialog, 8, HK_TG_MS_Method, HK_TG_MS);
	if (HK_TG_12_Method) RegisterHotKey(this->hwndDialog, 9, HK_TG_12_Method, HK_TG_12);

	// enable/disable mode radiobuttons
	::EnableWindow(::GetDlgItem(this->hwndDialog, 8300), this->ActiveMode);
	::EnableWindow(::GetDlgItem(this->hwndDialog, 8301), this->ActiveMode);
	::EnableWindow(::GetDlgItem(this->hwndDialog, 8302), this->ActiveMode);
	::EnableWindow(::GetDlgItem(this->hwndDialog, 8310), this->ActiveMode);

	// make it call HandleControl initially
	::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);

	m_fanTimer = ::SetTimer(this->hwndDialog, 1, this->Cycle * 1000, NULL);           // fan update
	m_titleTimer = ::SetTimer(this->hwndDialog, 2, 500, NULL);                        // title update
	m_iconTimer = ::SetTimer(this->hwndDialog, 3, this->IconCycle * 1000, NULL);      // Vista icon update
	if (this->ReIcCycle)
		m_renewTimer = ::SetTimer(this->hwndDialog, 4, this->ReIcCycle * 1000, NULL); // Vista icon update

	if (this->StartMinimized)
		::ShowWindow(this->hwndDialog, this->MinimizeToSysTray ? SW_HIDE : SW_MINIMIZE);
	else
		::ShowWindow(this->hwndDialog, TRUE);
}

//-------------------------------------------------------------------------
//  destructor
//-------------------------------------------------------------------------
FANCONTROL::~FANCONTROL() {
	if (this->hThread) {
		::WaitForSingleObject(this->hThread, 2000);
		this->hThread = NULL;
	}

	if (this->ModernS0Mode && this->hEventSubscription) {
		EvtClose(this->hEventSubscription);
		this->hEventSubscription = NULL;
	}

	if (this->PowerSuspendMode) {
		UnregisterPowerSettingNotification(this->hPowerNotify);
	}

	if (this->pTaskbarIcon) {
		delete this->pTaskbarIcon;
		this->pTaskbarIcon = NULL;
	}

	if (this->ppTbTextIcon) {
		delete ppTbTextIcon[0];
		delete[] ppTbTextIcon;
		ppTbTextIcon = NULL;
	}

	if (this->hwndDialog)
		::DestroyWindow(this->hwndDialog);

	if (pTextIconMutex)
		delete pTextIconMutex;
}

//-------------------------------------------------------------------------
//  Event log callback for Modern Standby events
//-------------------------------------------------------------------------
DWORD WINAPI FANCONTROL::EventLogCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent) {
	FANCONTROL* pThis = static_cast<FANCONTROL*>(pContext);

	if (action == EvtSubscribeActionDeliver && pThis != NULL) {
		pThis->HandleModernStandbyEvent(hEvent);
	}

	return ERROR_SUCCESS;
}

void FANCONTROL::HandleModernStandbyEvent(EVT_HANDLE hEvent) {
	DWORD bufferSize = 0;
	DWORD bufferUsed = 0;
	DWORD propertyCount = 0;

	// First call to get required buffer size
	EvtRender(NULL, hEvent, EvtRenderEventXml, bufferSize, NULL, &bufferUsed, &propertyCount);
	bufferSize = bufferUsed;

	std::vector<wchar_t> buffer(bufferSize / sizeof(wchar_t) + 1);
	if (EvtRender(NULL, hEvent, EvtRenderEventXml, bufferSize, buffer.data(), &bufferUsed, &propertyCount)) {
		// Parse for EventID - simple string search
		std::wstring xml(buffer.data());

		if (xml.find(L"<EventID>506</EventID>") != std::wstring::npos) {
			this->isModernS0State = true;
			this->Trace("Detected Modern S0 Entry");
			this->savedMode = this->CurrentMode;

			if (this->ModernS0Mode == 1) {
				this->ModeToDialog(1);
				if (this->SetFan("Switched to BIOS mode", 0x80)) ::Sleep(1000);
			}
			else if (this->ModernS0Mode == 2 || this->ModernS0Mode >= 4) {
				this->Trace("Continuing current mode");
			}
			else if (this->ModernS0Mode == 3) {
				this->ModeToDialog(3);
				if (this->SetFan("Switched fans off and to manual mode", 0x00))	::Sleep(1000);
			}
		}
		else if (xml.find(L"<EventID>507</EventID>") != std::wstring::npos) {
			this->isModernS0State = false;
			this->Trace("Detected Modern S0 Exit, defer access to EC (10s)");
			::Sleep(10000);

			if (this->savedMode != -1 && this->savedMode != this->CurrentMode) {
				this->ModeToDialog(this->savedMode);
				this->Trace("Restored saved mode");
			}
		}
	}
}

//-------------------------------------------------------------------------
//  mode integer from mode radio buttons
//-------------------------------------------------------------------------
int FANCONTROL::CurrentModeFromDialog() {
	BOOL modetpauto = ::SendDlgItemMessage(this->hwndDialog, 8300, BM_GETCHECK, 0L, 0L),
		modefcauto = ::SendDlgItemMessage(this->hwndDialog, 8301, BM_GETCHECK, 0L, 0L),
		modemanual = ::SendDlgItemMessage(this->hwndDialog, 8302, BM_GETCHECK, 0L, 0L);

	if (modetpauto)
		this->CurrentMode = 1;
	else if (modefcauto)
		this->CurrentMode = 2;
	else if (modemanual)
		this->CurrentMode = 3;
	else
		this->CurrentMode = -1;

	return this->CurrentMode;
}

int FANCONTROL::ShowAllFromDialog() {
	BOOL modefcauto = ::SendDlgItemMessage(this->hwndDialog, 7001, BM_GETCHECK, 0L, 0L),
		modemanual = ::SendDlgItemMessage(this->hwndDialog, 7002, BM_GETCHECK, 0L, 0L);

	if (modefcauto)
		this->ShowAll = 1;
	else if (modemanual)
		this->ShowAll = 0;
	else
		this->ShowAll = -1;

	return this->ShowAll;
}

void FANCONTROL::ModeToDialog(int mode) const {
	::SendDlgItemMessage(this->hwndDialog, 8300, BM_SETCHECK, mode == 1, 0L);
	::SendDlgItemMessage(this->hwndDialog, 8301, BM_SETCHECK, mode == 2, 0L);
	::SendDlgItemMessage(this->hwndDialog, 8302, BM_SETCHECK, mode == 3, 0L);
}

void FANCONTROL::ShowAllToDialog(int show) const {
	::SendDlgItemMessage(this->hwndDialog, 7001, BM_SETCHECK, show == 1, 0L);
	::SendDlgItemMessage(this->hwndDialog, 7002, BM_SETCHECK, show == 0, 0L);
}

//-------------------------------------------------------------------------
//  process main dialog
//-------------------------------------------------------------------------
int FANCONTROL::ProcessDialog() const {

	MSG qmsg, qmsg2;
	int dlgrc = -1;

	if (this->hwndDialog) {
		for (;;) {
			BOOL nodlgmsg = FALSE;

			::GetMessage(&qmsg, NULL, 0L, 0L);

			if (qmsg.message != WM__DISMISSDLG && IsDialogMessage(this->hwndDialog, &qmsg))
				continue;

			qmsg2 = qmsg;
			TranslateMessage(&qmsg);
			DispatchMessage(&qmsg);

			if (qmsg2.message == WM__DISMISSDLG && qmsg2.hwnd == this->hwndDialog) {
				dlgrc = qmsg2.wParam;
				break;
			}
		}
	}

	return dlgrc;
}

//-------------------------------------------------------------------------
//  dialog window procedure (map to class method)
//-------------------------------------------------------------------------
ULONG CALLBACK FANCONTROL::BaseDlgProc(HWND hwnd, ULONG msg, WPARAM mp1, LPARAM mp2) {
	ULONG rc = FALSE;

	static UINT s_TaskbarCreated;

	if (msg == WM_INITDIALOG) {
		s_TaskbarCreated = RegisterWindowMessage("TaskbarCreated");
	}

	FANCONTROL* This = (FANCONTROL*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (This) {
		if (msg == s_TaskbarCreated) {
			This->TaskbarNew = 1;

			if (This->pTaskbarIcon)
				This->pTaskbarIcon->RebuildIfNecessary(TRUE);
			else {
				This->RemoveTextIcons();
				This->ProcessTextIcons();
			}
		}
		rc = This->DlgProc(hwnd, msg, mp1, mp2);
	}

	return rc;
}

//-------------------------------------------------------------------------
//  file-scope state (persists across timer ticks)
//-------------------------------------------------------------------------
constexpr auto WANTED_MEM_SIZE = 65536*12;
BOOL dioicon(TRUE);
char szBuffer[BUFFER_SIZE];
char str_value[256];
DWORD cbBytes;
BOOL bResult(FALSE);
BOOL lbResult(FALSE);
int fanspeed;
int fanctrl;
int IconFontSize;
BOOL _piscreated(FALSE);
char obuftd[256] = "", obuftd2[128] = "", templisttd[512];
char obuf[256] = "", obuf2[128] = "", templist2[512];

// Temperature color constants
static constexpr COLORREF COLOR_RED_ORANGE = RGB(255, 69, 0);
static constexpr COLORREF COLOR_ORANGE = RGB(255, 165, 0);
static constexpr COLORREF COLOR_DARK_YELLOW = RGB(210, 160, 0);
static constexpr COLORREF COLOR_BLACK = RGB(0, 0, 0);

// Helper function to determine color based on temperature and thresholds
static inline COLORREF GetTempColor(int temp, const int* iconLevels) {
	if (temp >= iconLevels[2] && iconLevels[2] > 0)
		return COLOR_RED_ORANGE;
	if (temp >= iconLevels[1] && iconLevels[1] > 0)
		return COLOR_ORANGE;
	if (temp >= iconLevels[0] && iconLevels[0] > 0)
		return COLOR_DARK_YELLOW;
	return COLOR_BLACK;
}

//-------------------------------------------------------------------------
//  custom title bar (Gorilla fork)
//
//  Windows draws its own caption buttons and lets no program recolour their
//  hover; on Windows 11 minimise only turns a faint grey. The main dialogs
//  therefore have no WS_CAPTION and carry their own strip: title text 8120,
//  minimise 8121 and close 8122 (owner-drawn). Minimise hovers in
//  CaptionHoverColor (ini), close in the usual red. The strip drags the window.
//-------------------------------------------------------------------------
static const char GFC_HOVER[] = "GFC_Hover";
static const int  TITLE_STRIP_DLU = 16;

static LRESULT CALLBACK CaptionButtonProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
	switch (msg) {
	case WM_MOUSEMOVE:
		if (!::GetPropA(h, GFC_HOVER)) {
			::SetPropA(h, GFC_HOVER, (HANDLE)1);
			TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, h, 0 };
			::TrackMouseEvent(&t);
			::InvalidateRect(h, NULL, FALSE);
		}
		break;
	case WM_MOUSELEAVE:
		::RemovePropA(h, GFC_HOVER);
		::InvalidateRect(h, NULL, FALSE);
		break;
	case WM_NCDESTROY:
		::RemovePropA(h, GFC_HOVER);
		::RemoveWindowSubclass(h, CaptionButtonProc, 1);
		break;
	}
	return ::DefSubclassProc(h, msg, wp, lp);
}

static void InitCustomTitleBar(HWND dlg) {
	if (!::GetDlgItem(dlg, 8121)) return;        // slim dialogs keep the Windows caption

	// A CAPTION statement in a dialog template switches WS_CAPTION back on, so
	// the Windows title bar is removed here, keeping the client area's size.
	LONG_PTR style = ::GetWindowLongPtr(dlg, GWL_STYLE);
	if (style & WS_CAPTION) {
		RECT client; ::GetClientRect(dlg, &client);
		style &= ~(LONG_PTR)WS_CAPTION;
		::SetWindowLongPtr(dlg, GWL_STYLE, style);
		RECT want = client;
		::AdjustWindowRectEx(&want, (DWORD)style, FALSE, (DWORD)::GetWindowLongPtr(dlg, GWL_EXSTYLE));
		::SetWindowPos(dlg, NULL, 0, 0, want.right - want.left, want.bottom - want.top,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	}

	for (int id = 8121; id <= 8122; id++) {
		HWND b = ::GetDlgItem(dlg, id);
		if (b) ::SetWindowSubclass(b, CaptionButtonProc, 1, 0);
	}
	static HFONT s_bold = NULL;                  // one font for the process
	HWND t = ::GetDlgItem(dlg, 8120);
	HFONT f = (HFONT)::SendMessage(dlg, WM_GETFONT, 0, 0);
	if (t && f) {
		if (!s_bold) {
			LOGFONT lf;
			if (::GetObject(f, sizeof(lf), &lf)) { lf.lfWeight = FW_SEMIBOLD; s_bold = ::CreateFontIndirect(&lf); }
		}
		if (s_bold) ::SendMessage(t, WM_SETFONT, (WPARAM)s_bold, TRUE);
	}
}

static int TitleStripHeightPx(HWND dlg) {
	RECT r = { 0, 0, 0, TITLE_STRIP_DLU };
	::MapDialogRect(dlg, &r);
	return r.bottom;
}

static BOOL DrawCaptionButton(const DRAWITEMSTRUCT* d, COLORREF minHover) {
	const bool isClose = d->CtlID == 8122;
	const bool hover = ::GetPropA(d->hwndItem, GFC_HOVER) != NULL;
	const bool pressed = (d->itemState & ODS_SELECTED) != 0;
	const COLORREF hot = isClose ? RGB(0xE8, 0x11, 0x23) : minHover;
	COLORREF bg = ::GetSysColor(COLOR_BTNFACE);
	if (pressed)
		bg = RGB(GetRValue(hot) * 3 / 4, GetGValue(hot) * 3 / 4, GetBValue(hot) * 3 / 4);
	else if (hover)
		bg = hot;
	HBRUSH br = ::CreateSolidBrush(bg);
	::FillRect(d->hDC, &d->rcItem, br);
	::DeleteObject(br);

	const COLORREF ink = (hover || pressed) ? RGB(0xFF, 0xFF, 0xFF) : ::GetSysColor(COLOR_BTNTEXT);
	const int w = d->rcItem.right - d->rcItem.left, h = d->rcItem.bottom - d->rcItem.top;
	int s = h * 2 / 5; if (s < 6) s = 6;          // glyph size grows with display scaling
	const int cx = d->rcItem.left + w / 2, cy = d->rcItem.top + h / 2;
	HPEN pen = ::CreatePen(PS_SOLID, h >= 28 ? 2 : 1, ink);
	HGDIOBJ old = ::SelectObject(d->hDC, pen);
	if (isClose) {
		::MoveToEx(d->hDC, cx - s / 2, cy - s / 2, NULL); ::LineTo(d->hDC, cx + s / 2 + 1, cy + s / 2 + 1);
		::MoveToEx(d->hDC, cx + s / 2, cy - s / 2, NULL); ::LineTo(d->hDC, cx - s / 2 - 1, cy + s / 2 + 1);
	}
	else {
		::MoveToEx(d->hDC, cx - s / 2, cy, NULL); ::LineTo(d->hDC, cx + s / 2 + 1, cy);
	}
	::SelectObject(d->hDC, old);
	::DeleteObject(pen);
	return TRUE;
}

//-------------------------------------------------------------------------
//  dialog window procedure — thin dispatcher
//-------------------------------------------------------------------------
ULONG FANCONTROL::DlgProc(HWND hwnd, ULONG msg, WPARAM mp1, LPARAM mp2) {
	switch (msg) {
	case WM_HOTKEY:
		return OnHotKey(mp1);

	case WM_INITDIALOG:
		// placing code here will NOT work!
		// (put it into BaseDlgProc instead)
		break;

	case WM_TIMER:
		return OnTimer(mp1);

	case WM_NOTIFY:
		{
			LPNMHDR pnmh = (LPNMHDR)mp2;
			// Only handle custom draw if control 8101 is a ListView (normal/StayOnTop dialogs)
			// In slim dialogs, control 8101 is an EDITTEXT
			if (!SlimDialog && pnmh->idFrom == 8101 && pnmh->code == NM_CUSTOMDRAW)
			{
				LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)mp2;
				LRESULT result = CDRF_DODEFAULT;

				switch (lplvcd->nmcd.dwDrawStage)
				{
				case CDDS_PREPAINT:
					result = CDRF_NOTIFYITEMDRAW;
					break;
				case CDDS_ITEMPREPAINT:
					result = CDRF_NOTIFYSUBITEMDRAW;
					break;
				case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
				{
					lplvcd->clrTextBk = CLR_DEFAULT;
					if (lplvcd->iSubItem == 2)
					{
						char tempStr[16];
						ListView_GetItemText(pnmh->hwndFrom, (int)lplvcd->nmcd.dwItemSpec, 2, tempStr, sizeof(tempStr));
						int temp = atoi(tempStr);
						lplvcd->clrText = GetTempColor(temp, this->IconLevels);
					}
					else
					{
						lplvcd->clrText = CLR_DEFAULT;
					}
					result = CDRF_NEWFONT;
					break;
				}
				default:
					result = CDRF_DODEFAULT;
					break;
				}
				SetWindowLongPtr(hwnd, DWLP_MSGRESULT, result);
				return TRUE;
			}
			else if (!SlimDialog && pnmh->code == NM_CUSTOMDRAW)
			{
				HWND hLV = ::GetDlgItem(hwnd, 8101);
				if (hLV)
				{
					HWND hHeader = ListView_GetHeader(hLV);
					if (pnmh->hwndFrom == hHeader)
					{
						LPNMCUSTOMDRAW lpnmcd = (LPNMCUSTOMDRAW)mp2;
						LRESULT result = CDRF_DODEFAULT;

						switch (lpnmcd->dwDrawStage)
						{
						case CDDS_PREPAINT:
							result = CDRF_NOTIFYITEMDRAW;
							break;
						case CDDS_ITEMPREPAINT:
						{
							if (lpnmcd->dwItemSpec == 2)
							{
								COLORREF textColor = GetTempColor(this->MaxTemp, this->IconLevels);
								SetTextColor(lpnmcd->hdc, textColor);
								result = CDRF_NEWFONT;
							}
							break;
						}
						default:
							result = CDRF_DODEFAULT;
							break;
						}
						SetWindowLongPtr(hwnd, DWLP_MSGRESULT, result);
						return TRUE;
					}
				}
			}
		}
		break;

	case WM_COMMAND:
		return OnCommand(mp1);

	case WM_DRAWITEM:   // custom title bar buttons
		if (mp1 == 8121 || mp1 == 8122)
			return DrawCaptionButton((const DRAWITEMSTRUCT*)mp2, this->CaptionHoverColor);
		break;

	case WM_NCHITTEST: {  // the custom title strip moves the window, like a caption
		POINT pt = { (short)LOWORD(mp2), (short)HIWORD(mp2) };
		::ScreenToClient(hwnd, &pt);
		if (pt.y >= 0 && pt.y < TitleStripHeightPx(hwnd) && ::GetDlgItem(hwnd, 8121)) {
			HWND child = ::ChildWindowFromPointEx(hwnd, pt, CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT);
			if (child == NULL || child == hwnd || child == ::GetDlgItem(hwnd, 8120)) {
				::SetWindowLongPtr(hwnd, DWLP_MSGRESULT, HTCAPTION);
				return TRUE;
			}
		}
		break;
	}

	case WM_CLOSE:
		// close = hide to the tray; fan control keeps running (Exit is in the tray menu)
		::ShowWindow(this->hwndDialog, this->MinimizeToSysTray ? SW_HIDE : SW_MINIMIZE);
		return TRUE;

	case WM_POWERBROADCAST:
		return OnPowerBroadcast(mp1, mp2);

	case WM_ENDSESSION:
		return OnEndSession();

	case WM_SIZE:
		// minimise now goes to the taskbar like any window; hiding to the tray
		// is what close, the tray icon and "Hide window" do (Gorilla fork)
		return TRUE;

	case WM_DESTROY:
		break;

	case WM__GETDATA:
		// a client has no port access, so it hands the change to the engine
		if (g_clientMode)
			this->SendCommand(-1);
		else if (!this->hThread && !this->FinalSeen)
			this->hThread = this->CreateThread(FANCONTROL_Thread, (ULONG)this);
		break;

	case WM__NEWDATA:
		return OnNewData(mp1);

	case WM__TASKBAR:
		return OnTaskbarNotify(mp2);

	default:
		break;
	}

	return 0;
}

//-------------------------------------------------------------------------
//  WM_HOTKEY handler
//-------------------------------------------------------------------------
ULONG FANCONTROL::OnHotKey(WPARAM mp1) {
	switch (mp1) {
	case 1: // BIOS mode
		this->ModeToDialog(1);
		::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
		break;

	case 2: // Smart mode
		this->ModeToDialog(2);
		::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
		break;

	case 3: // Manual mode
		this->ModeToDialog(3);
		::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
		break;

	case 4: // Smart Mode 1
		SwitchSmartLevel(0);
		break;

	case 5: // Smart Mode 2
		SwitchSmartLevel(1);
		break;

	case 6: // Toggle BIOS <-> Smart
		if (this->CurrentMode > 1) {
			this->ModeToDialog(1);
		}
		else {
			this->ModeToDialog(2);
		}
		::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
		break;

	case 7: // Toggle BIOS <-> Manual
		if (this->CurrentMode > 1) {
			this->ModeToDialog(1);
		}
		else {
			this->ModeToDialog(3);
		}
		::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
		break;

	case 8: // Toggle Manual <-> Smart
		if (this->CurrentMode < 3) {
			this->ModeToDialog(3);
		}
		else {
			this->ModeToDialog(2);
		}
		::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
		break;

	case 9: // Toggle Smart Mode 1 <-> 2
		SwitchSmartLevel(this->IndSmartLevel == 0 ? 1 : 0);
		break;
	}

	// hotkeys 1-3 and the toggles 6-8 are a user's choice of mode: remember it
	if (!g_clientMode && ((mp1 >= 1 && mp1 <= 3) || (mp1 >= 6 && mp1 <= 8))) {
		char level[64] = "";
		::GetWindowTextA(::GetDlgItem(this->hwndDialog, 8310), level, sizeof(level));
		this->PersistUserMode(this->CurrentModeFromDialog(), level);
	}

	return 0;
}

//-------------------------------------------------------------------------
//  WM_TIMER handler
//-------------------------------------------------------------------------
ULONG FANCONTROL::OnTimer(WPARAM timerId) {
	// an outside shutdown request, taken through TryClose so the BIOS gets the fan back
	static HANDLE closeEvent = CreateSharedEvent("Global\\TPFanControl_Close");
	if (closeEvent && ::WaitForSingleObject(closeEvent, 0) == WAIT_OBJECT_0) {
		// the engine clears it, so the next instance is not shut down by a stale request
		if (!g_clientMode)
			::ResetEvent(closeEvent);

		this->Trace("Close requested by another instance");
		TryClose();
		return 0;
	}

	switch (timerId) {
	case 1: // update fan state
	{
		// WM__GETDATA starts an EC read, so a client draws the published state instead
		if (g_clientMode)
			this->PullSharedState();
		else
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);

		if (this->Log2csv == 1)
			this->Tracecsv(this->CurrentStatuscsv);
		break;
	}

	case 2: { // update window title
		if (this->CurrentMode == 3 && this->MaxTemp > this->ManModeExitInternal) {
			this->ModeToDialog(2);
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
		}

		ULONG res = this->IsMinimized();
		if (res && strcmp(this->LastTitle, this->Title2) != 0) {
			strcpy_s(this->LastTitle, sizeof(this->LastTitle), this->Title2);
		}
		else if (!res && strcmp(this->LastTitle, this->Title) != 0) {
			::SetWindowText(this->hwndDialog, this->Title);
			::SetDlgItemText(this->hwndDialog, 8120, this->Title);   // custom title strip
			strcpy_s(this->LastTitle, sizeof(this->LastTitle), this->Title);
		}

		if (this->pTaskbarIcon) {
			this->pTaskbarIcon->SetTooltip(this->Title2);
			strcpy_s(this->LastTooltip, sizeof(this->LastTooltip), this->Title2);
			int icon = -1;

			if (this->CurrentModeFromDialog() == 1) {
				icon = 10;    // gray
			}
			else {
				icon = 11;    // blue
				for (int i = 0; i < ARRAYMAX(this->IconLevels); i++) {
					if (this->MaxTemp >= this->IconLevels[i]) {
						icon = 12 + i;    // yellow, orange, red
					}
				}
			}

			if (icon != this->CurrentIcon && icon != -1) {
				this->pTaskbarIcon->SetIcon(icon);
				this->CurrentIcon = icon;
				if (dioicon && !this->NoBallons) {
					this->pTaskbarIcon->SetBalloon(NIIF_INFO, "TPFanControl old symbol icon",
						"shows temperature level by color and state in tooltip, left click on icon shows or hides control window, right click shows menue",
						11);
					dioicon = FALSE;
				}
			}
			this->iFarbeIconB = icon;
		}
		break;
	}

	case 3: { // update vista icon / named pipe
		// the engine already publishes on this pipe name, a client must not
		if (g_clientMode)
			break;

		// Reconnect if previous write failed
		if (bResult == FALSE && lbResult == TRUE) {
			_piscreated = FALSE;
			lbResult = FALSE;
			bResult = FALSE;
			CloseAllNamedPipes();
		}

		if (_piscreated == FALSE) {
			CreateAllNamedPipes();
			_piscreated = TRUE;
		}

		// Build pipe data
		if (fan1speed > 0x1fff)
			fan1speed = lastfan1speed;

		char pipeData[BUFFER_SIZE];
		if (Fahrenheit) {
			sprintf_s(pipeData, sizeof(pipeData), "%d %d %s %d %d %d ",
				this->CurrentMode, (this->MaxTemp * 9 / 5 + 32), this->gSensorNames[iMaxTemp],
				iFarbeIconB, fan1speed, fanctrl2);
		}
		else {
			sprintf_s(pipeData, sizeof(pipeData), "%d %d %s %d %d %d ",
				this->CurrentMode, (this->MaxTemp), this->gSensorNames[iMaxTemp],
				iFarbeIconB, fan1speed, fanctrl2);
		}

		WriteAllNamedPipes(pipeData);
		break;
	}

	case 4: // renew tempicon
		if (ShowTempIcon && ReIcCycle) {
			this->RemoveTextIcons();
			this->ProcessTextIcons();
		}
		break;

	default:
		break;
	}

	if (this->ShowTempIcon == 1)
		this->ProcessTextIcons();
	else
		this->RemoveTextIcons();

	return 0;
}

//-------------------------------------------------------------------------
//  autostart: the service owns the fan from boot, a Run entry brings up the
//  tray window in whichever session logs on. Neither works alone.
//-------------------------------------------------------------------------
DWORD InstallService(bool quiet);
DWORD UninstallService(bool quiet);
bool IsElevated();
bool RunSelfElevated(const char* args, bool wait);

static const char* RUN_KEY = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";

static bool AutostartEnabled() {
	SC_HANDLE mgr = ::OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (!mgr)
		return false;

	SC_HANDLE svc = ::OpenService(mgr, "TPFanControl", SERVICE_QUERY_STATUS);
	bool on = (svc != NULL);
	if (svc)
		::CloseServiceHandle(svc);
	::CloseServiceHandle(mgr);

	return on;
}

// versions up to 2.5.0 used a run key entry, but Windows 11 was seen
// skipping those at logon with nothing logged about why, so the Startup
// folder shortcut owns the job now and any leftover entry is removed on
// either direction of the toggle
static void DeleteLegacyRunEntry() {
	HKEY key;
	if (::RegOpenKeyEx(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
		return;

	::RegDeleteValue(key, "TPFanControl");
	::RegCloseKey(key);
}

static bool StartupShortcutPath(char* path, size_t size) {
	if (::SHGetFolderPathA(NULL, CSIDL_STARTUP, NULL, SHGFP_TYPE_CURRENT, path) != S_OK)
		return false;

	return strcat_s(path, size, "\\TPFanControl.lnk") == 0;
}

static bool CreateStartupShortcut() {
	char lnk[MAX_PATH + 32];
	if (!StartupShortcutPath(lnk, sizeof(lnk)))
		return false;

	char exe[MAX_PATH];
	::GetModuleFileName(NULL, exe, MAX_PATH);

	// the shortcut carries the working directory, so the ini and the log
	// resolve to the program folder no matter who launches it
	char dir[MAX_PATH];
	strcpy_s(dir, sizeof(dir), exe);
	char* slash = strrchr(dir, '\\');
	if (slash) *slash = 0;

	const HRESULT com = ::CoInitialize(NULL);

	IShellLinkA* link = NULL;
	bool ok = false;

	if (SUCCEEDED(::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
			IID_IShellLinkA, (void**)&link))) {
		link->SetPath(exe);
		link->SetWorkingDirectory(dir);
		link->SetDescription("TPFanControl tray");

		IPersistFile* file = NULL;
		if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&file))) {
			wchar_t wlnk[MAX_PATH + 32];
			::MultiByteToWideChar(CP_ACP, 0, lnk, -1, wlnk, MAX_PATH + 32);
			ok = SUCCEEDED(file->Save(wlnk, TRUE));
			file->Release();
		}

		link->Release();
	}

	if (SUCCEEDED(com))
		::CoUninitialize();

	return ok;
}

static bool DeleteStartupShortcut() {
	char lnk[MAX_PATH + 32];
	if (!StartupShortcutPath(lnk, sizeof(lnk)))
		return false;

	if (::DeleteFileA(lnk))
		return true;

	return ::GetLastError() == ERROR_FILE_NOT_FOUND;
}

static bool SetStartupShortcut(bool on) {
	DeleteLegacyRunEntry();

	return on ? CreateStartupShortcut() : DeleteStartupShortcut();
}

static bool SetAutostart(bool on) {
	// the shortcut is this user's own, the service needs elevation
	bool svc = IsElevated()
		? (on ? InstallService(true) : UninstallService(true)) == 0
		: RunSelfElevated(on ? "-i -q" : "-u -q", true);

	// a shortcut with no service behind it would prompt for elevation at every logon
	if (on)
		return svc && SetStartupShortcut(true);

	return SetStartupShortcut(false) && svc;
}

//-------------------------------------------------------------------------
//  WM_COMMAND handler
//-------------------------------------------------------------------------
ULONG FANCONTROL::OnCommand(WPARAM mp1) {
	if (HIWORD(mp1) != BN_CLICKED && HIWORD(mp1) != EN_CHANGE
		&& HIWORD(mp1) != CBN_SELCHANGE && HIWORD(mp1) != CBN_EDITCHANGE)
		return 0;

	int cmd = LOWORD(mp1);

	if (cmd == 7001 || cmd == 7002) {
		this->ShowAllFromDialog();
		this->UpdateTempDisplay();
	}

	if (cmd >= 8300 && cmd <= 8302 || cmd == 8310) {  // radio button or manual speed entry
		char level[64] = "";
		if (cmd == 8310) {  // auto-switch to Manual when user interacts with speed ComboBox
			if (HIWORD(mp1) == CBN_EDITCHANGE)  // ignore per-keystroke, only act on CBN_SELCHANGE
				return 0;
			this->ModeToDialog(3);
			// On CBN_SELCHANGE the edit text is not updated yet: read the picked item.
			HWND hCB = ::GetDlgItem(this->hwndDialog, 8310);
			LRESULT sel = hCB ? ::SendMessageA(hCB, CB_GETCURSEL, 0, 0) : CB_ERR;
			if (sel != CB_ERR && ::SendMessageA(hCB, CB_GETLBTEXTLEN, sel, 0) < (LRESULT)sizeof(level))
				::SendMessageA(hCB, CB_GETLBTEXT, sel, (LPARAM)level);
		}
		if (!level[0])
			::GetWindowTextA(::GetDlgItem(this->hwndDialog, 8310), level, sizeof(level));
		if (!g_clientMode && HIWORD(mp1) != EN_CHANGE)
			this->PersistUserMode(this->CurrentModeFromDialog(), level);
		::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
	}
	else {
		switch (cmd) {
		case 8121: // custom title bar: minimise to the taskbar
			::ShowWindow(this->hwndDialog, SW_MINIMIZE);
			break;

		case 8122: // custom title bar: close = hide to the tray (fan control keeps running)
			::SendMessage(this->hwndDialog, WM_CLOSE, 0, 0);
			break;

		case 5001: // bios
			this->ModeToDialog(1);
			if (!g_clientMode) this->PersistUserMode(1, NULL);
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
			break;

		case 5002: // smart
			this->ModeToDialog(2);
			if (!g_clientMode) this->PersistUserMode(2, NULL);
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
			break;

		case 5003: // smart1
			SwitchSmartLevel(0);
			break;

		case 5004: // smart2
			SwitchSmartLevel(1);
			break;

		case 5005: // manual
			this->ModeToDialog(3);
			if (!g_clientMode) {
				char level[64] = "";
				::GetWindowTextA(::GetDlgItem(this->hwndDialog, 8310), level, sizeof(level));
				this->PersistUserMode(3, level);
			}
			::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
			break;

		case 5010: // show window
			::ShowWindow(this->hwndDialog, TRUE);
			::SetForegroundWindow(this->hwndDialog);
			break;

		case 5040: // bluetooth toggle
			if (BluetoothEDR)
				this->SetHdw("Bluetooth", 16, 58, 32);
			else
				this->SetHdw("Bluetooth", 32, 59, 16);
			break;

		case 5070: // show old icon
			this->ShowTempIcon = 0;
			this->pTaskbarIcon = new TASKBARICON(this->hwndDialog, 10, "TPFanControl");
			this->pTaskbarIcon->SetIcon(this->CurrentIcon);
			break;

		case 5080: // show temp icon
			delete this->pTaskbarIcon;
			this->pTaskbarIcon = NULL;
			this->ShowTempIcon = 1;
			break;

		case 5030: // hide window
			::ShowWindow(this->hwndDialog, this->MinimizeToSysTray ? SW_HIDE : SW_MINIMIZE);
			break;

		case 5050: // start with windows toggle
		{
			bool on = !AutostartEnabled();
			this->Trace(SetAutostart(on)
				? (on ? "Enabled start with Windows" : "Disabled start with Windows")
				: "Failed to change start with Windows");
			break;
		}

		case 5020: // end program
			// closing the window ends the engine too, which hands the fan back to the BIOS
			if (g_clientMode) {
				HANDLE stop = CreateSharedEvent("Global\\TPFanControl_Close");
				if (stop) {
					this->Trace("Asking the instance that owns the fan to close");
					::SetEvent(stop);
					::CloseHandle(stop);
				}
			}

			TryClose();
			break;
		}
	}

	return 0;
}

//-------------------------------------------------------------------------
//  WM_POWERBROADCAST handler
//-------------------------------------------------------------------------
ULONG FANCONTROL::OnPowerBroadcast(WPARAM mp1, LPARAM mp2) {
	if (mp1 == PBT_APMSUSPEND) {
		this->isPowerSuspendState = true;
		this->savedMode = this->CurrentMode;
		this->Trace("System suspend detected");

		if (this->PowerSuspendMode == 4) {
			this->ModeToDialog(3);
			if (this->SetFan("Switched fans off and to manual mode", 0x00)) ::Sleep(1000);
		}
	}
	else if (mp1 == PBT_APMRESUMEAUTOMATIC) {
		this->isPowerSuspendState = false;
		this->Trace("System resume detected");

		if (this->PowerSuspendMode == 4) {
			if (this->savedMode != -1 && this->savedMode != this->CurrentMode) {
				::Sleep(5000);
				this->ModeToDialog(this->savedMode);
				::Sleep(1000);
				this->Trace("Restored saved mode");
			}
		}
	}
	else if (mp1 == PBT_POWERSETTINGCHANGE) {
		POWERBROADCAST_SETTING* pbs = (POWERBROADCAST_SETTING*)mp2;
		if (pbs->PowerSetting == GUID_LIDSWITCH_STATE_CHANGE) {
			BYTE state = *(BYTE*)(&pbs->Data);

			if (state == 0) {  // Lid closed
				this->isLidClosed = true;
				this->Trace("Lid close detected");
				this->savedMode = this->CurrentMode;

				if (this->PowerSuspendMode == 1) {
					this->ModeToDialog(1);
					if (this->SetFan("Switched to BIOS mode", 0x80)) ::Sleep(1000);
				}
				else if (this->PowerSuspendMode == 2 || this->PowerSuspendMode >= 5) {
					this->Trace("Continuing current mode");

					// docked profile: run the other smart curve while the lid is closed
					if (!g_clientMode && this->LidSmartLevel && this->CurrentMode == 2
						&& this->SmartLevels2[0].temp2 != 0
						&& this->IndSmartLevel != this->LidSmartLevel - 1) {
						this->savedSmartLevel = this->IndSmartLevel;
						this->SwitchSmartLevel(this->LidSmartLevel - 1);
					}
				}
				else if (this->PowerSuspendMode == 3) {
					this->ModeToDialog(3);
					if (this->SetFan("Switched fans off and to manual mode", 0x00)) ::Sleep(1000);
				}
				else if (this->PowerSuspendMode == 4) {
					// Defer to PBT_APMSUSPEND/PBT_APMRESUMEAUTOMATIC handling
				}
			}
			else { // Lid opened
				this->isLidClosed = false;
				this->Trace("Lid open detected");

				if (this->PowerSuspendMode != 4) {
					if (this->savedMode != -1 && this->savedMode != this->CurrentMode) {
						::Sleep(5000);
						this->ModeToDialog(this->savedMode);
						::Sleep(1000);
						this->Trace("Restored saved mode");
					}
				}

				if (!g_clientMode && this->savedSmartLevel != -1) {
					this->SwitchSmartLevel(this->savedSmartLevel);
					this->savedSmartLevel = -1;
				}
			}
		}
	}

	return 0;
}

//-------------------------------------------------------------------------
//  WM_ENDSESSION handler
//-------------------------------------------------------------------------
ULONG FANCONTROL::OnEndSession() {
	if (!this->Runs_as_service) {
		TryClose();
	}
	return 0;
}

//-------------------------------------------------------------------------
//  WM__NEWDATA handler
//-------------------------------------------------------------------------
ULONG FANCONTROL::OnNewData(WPARAM mp1) {
	if (this->hThread) {
		::WaitForSingleObject(this->hThread, INFINITE);
		if (this->hThread)
			::CloseHandle(this->hThread);
		else {
			this->Trace("Exception detected, closing to BIOS mode");
			::SendMessage(this->hwndDialog, WM_ENDSESSION, 0, 0);
		}
		this->hThread = 0;
	}

	ULONG ok = mp1;  // equivalent of "ok = this->ReadEcStatus(&this->State);" via thread

	if (ok) {
		this->ReadErrorCount = 0;
		this->HandleData();

		if (m_needClose) {
			this->Trace("Program needs to be closed, changing to BIOS mode");
			::Sleep(1000);
			::PostMessage(this->hwndDialog, WM_COMMAND, 5020, 0);
			::SendMessage(this->hwndDialog, WM_ENDSESSION, 0, 0);
			m_needClose = false;
		}
	}
	else {
		char buf[1024];
		sprintf_s(buf, sizeof(buf), "Warning: can't read Status, read error count = %d", this->ReadErrorCount);
		this->Trace(buf);
		sprintf_s(buf, sizeof(buf), "We will close to BIOS-Mode after %d consecutive read errors", this->MaxReadErrors);
		this->Trace(buf);
		this->ReadErrorCount++;

		// after so many consecutive read errors, try to switch back to bios mode
		if (this->ReadErrorCount > this->MaxReadErrors) {
			this->ModeToDialog(1);
			ok = this->SetFan("Max. Errors", 0x80);
			if (ok) {
				this->Trace("Set to BIOS Mode, to many consecutive read errors");
				::Sleep(2000);
				::SendMessage(this->hwndDialog, WM_ENDSESSION, 0, 0);
			}
		}
	}

	return 0;
}

//-------------------------------------------------------------------------
//  WM__TASKBAR handler
//-------------------------------------------------------------------------
ULONG FANCONTROL::OnTaskbarNotify(LPARAM mp2) {
	switch (mp2) {
	case WM_LBUTTONDOWN:
		if (!IsWindowVisible(this->hwndDialog) || IsIconic(this->hwndDialog)) {
			::ShowWindow(this->hwndDialog, IsIconic(this->hwndDialog) ? SW_RESTORE : SW_SHOW);
			::SetForegroundWindow(this->hwndDialog);
		}
		else
			::ShowWindow(this->hwndDialog, this->MinimizeToSysTray ? SW_HIDE : SW_MINIMIZE);
		break;

	case WM_LBUTTONUP:
	{
		BOOL isshift = ::GetAsyncKeyState(VK_SHIFT) & 0x8000;
		BOOL isctrl = ::GetAsyncKeyState(VK_CONTROL) & 0x8000;

		int action = -1;

		// some fancy key dependent stuff could be done here.
	}
	break;

	case WM_LBUTTONDBLCLK:
		if (!IsWindowVisible(this->hwndDialog) || IsIconic(this->hwndDialog)) {
			::ShowWindow(this->hwndDialog, IsIconic(this->hwndDialog) ? SW_RESTORE : SW_SHOW);
			::SetForegroundWindow(this->hwndDialog);
		}
		else
			::ShowWindow(this->hwndDialog, this->MinimizeToSysTray ? SW_HIDE : SW_MINIMIZE);
		break;

	case WM_RBUTTONDOWN: {
		MENU m(5000);

		// a client has no EC backend, so the hardware toggles belong to the engine
		if (g_clientMode) {
			m.DisableMenuItem(5040);
			m.DisableMenuItem(5060);
		}
		else {
			if (!this->LockECAccess()) break;

			char testpara;
			ULONG ok = this->ReadByteFromEC(59, &testpara);
			if (testpara & 2) m.CheckMenuItem(5060);

			if (this->BluetoothEDR) {
				ok = this->ReadByteFromEC(58, &testpara);
				if (testpara & 16) m.CheckMenuItem(5040);
			}
			else {
				ok = this->ReadByteFromEC(59, &testpara);
				if (testpara & 32) m.CheckMenuItem(5040);
			}

			this->FreeECAccess();
		}

		int mode = this->CurrentModeFromDialog();
		if (mode == 1) {
			m.CheckMenuItem(5001);
			if (this->ActiveMode == 0) {
				m.DisableMenuItem(5002);
				m.DisableMenuItem(5003);
				m.DisableMenuItem(5004);
				m.DisableMenuItem(5005);
			}
		}
		else if (mode == 2)
			m.CheckMenuItem(5002);
		else if (mode == 3)
			m.CheckMenuItem(5005);

		m.InsertItem(this->MenuLabelSM1, 5003, 10);
		m.InsertItem(this->MenuLabelSM2, 5004, 11);

		if (this->SmartLevels2[0].temp2 == 0) {
			m.DeleteMenuItem(5003);
			m.DeleteMenuItem(5004);
		}
		else {
			m.DeleteMenuItem(5002);

			if (mode == 2) {
				if (this->IndSmartLevel == 0)
					m.CheckMenuItem(5003);
				else
					m.CheckMenuItem(5004);
			}
		}

		if (Runs_as_service)
			m.DeleteMenuItem(5020);

		if (IsWindowVisible(this->hwndDialog))
			m.DeleteMenuItem(5010);
		else
			m.DeleteMenuItem(5030);

		if (this->ShowTempIcon == 0)
			m.DeleteMenuItem(5070);
		else
			m.DeleteMenuItem(5080);

		if (Runs_as_service)
			m.DeleteMenuItem(5050);
		else if (AutostartEnabled())
			m.CheckMenuItem(5050);

		m.Popup(this->hwndDialog);
		break;
	}
	}

	return TRUE;
}

//-------------------------------------------------------------------------
//  attempt graceful shutdown: set fan to BIOS, kill timers, dismiss dialog
//-------------------------------------------------------------------------
bool FANCONTROL::TryClose() {
	// Wait for the work thread to terminate
	if (this->hThread) {
		::WaitForSingleObject(this->hThread, INFINITE);
	}

	if (!this->EcAccess.Lock(100)) {
		// Something is going on, let's do this later
		this->Trace("Delaying close");
		m_needClose = true;
		return false;
	}

	// don't close if we can't set the fan back to bios controlled
	// a client has no fan to hand back, and SetFan refuses for it anyway
	if (g_clientMode || !this->ActiveMode || this->SetFan("On close", 0x80, true)) {
		::KillTimer(this->hwndDialog, m_fanTimer);
		::KillTimer(this->hwndDialog, m_titleTimer);
		::KillTimer(this->hwndDialog, m_iconTimer);
		::KillTimer(this->hwndDialog, m_renewTimer);
		BOOL CloHT = CloseHandle(this->hThread);
		this->Trace("Exiting ProcessDialog");
		this->EcAccess.Unlock();
		::PostMessage(this->hwndDialog, WM__DISMISSDLG, IDCANCEL, 0); // exit from ProcessDialog()
		return true;
	}

	m_needClose = true;
	this->EcAccess.Unlock();
	return false;
}

//-------------------------------------------------------------------------
//  switch to smart level 0 (SM1) or 1 (SM2), with logging
//-------------------------------------------------------------------------
void FANCONTROL::SwitchSmartLevel(int level) {
	this->ModeToDialog(2);

	// the profile tables live in the engine, a client only names the one it wants
	if (g_clientMode) {
		this->IndSmartLevel = level;
		this->SendCommand(level);
		return;
	}

	if (level == 0 && this->IndSmartLevel != 0) {
		this->Trace("Activation of Fan Control Profile 'Smart Mode 1'");
	}
	else if (level == 1 && this->IndSmartLevel != 1) {
		this->Trace("Activation of Fan Control Profile 'Smart Mode 2'");
	}

	this->IndSmartLevel = level;

	for (int i = 0; i < 32; i++) {
		if (level == 0) {
			this->SmartLevels[i].temp = this->SmartLevels1[i].temp1;
			this->SmartLevels[i].fan = this->SmartLevels1[i].fan1;
		}
		else {
			this->SmartLevels[i].temp = this->SmartLevels2[i].temp2;
			this->SmartLevels[i].fan = this->SmartLevels2[i].fan2;
		}
	}

	::PostMessage(this->hwndDialog, WM__GETDATA, 0, 0);
}

//-------------------------------------------------------------------------
//  client: draw what the engine last published
//-------------------------------------------------------------------------
void FANCONTROL::PullSharedState() {
	FCSHARED* shared = SharedState();
	if (!shared)
		return;

	// mirror engine log lines even when the state itself has not moved
	const LONG ringmax = (LONG)ARRAYMAX(shared->traceLines);
	LONG traceSeq = shared->traceSeq;
	if (traceSeq > this->LastTraceSeq) {
		LONG from = traceSeq - this->LastTraceSeq > ringmax
			? traceSeq - ringmax + 1 : this->LastTraceSeq + 1;
		for (LONG s = from; s <= traceSeq; s++)
			this->TraceAppend(shared->traceLines[s % ringmax]);
		this->LastTraceSeq = traceSeq;
	}

	if (shared->stateSeq == this->LastStateSeq)
		return;

	this->LastStateSeq = shared->stateSeq;

	this->State.FanCtrl     = (char)shared->fanCtrl;
	this->State.Fan1SpeedLo = (char)shared->fan1lo;
	this->State.Fan1SpeedHi = (char)shared->fan1hi;
	this->State.Fan2SpeedLo = (char)shared->fan2lo;
	this->State.Fan2SpeedHi = (char)shared->fan2hi;

	for (int i = 0; i < 12; i++) {
		this->State.Sensors[i] = (char)shared->sensors[i];
		// ReadEcStatus never runs in a client, the name pointers stay null without this
		this->State.SensorName[i] = this->gSensorNames[i];
	}

	// follow the engine's mode once it has acted on everything we asked for
	if (shared->ackSeq >= this->LastCmdSeq) {
		if (shared->mode != this->CurrentModeFromDialog())
			this->ModeToDialog(shared->mode);
		this->IndSmartLevel = shared->smartLevel;
	}

	::PostMessage(this->hwndDialog, WM__NEWDATA, 1, 0);
}

//-------------------------------------------------------------------------
//  client: hand the user's choice to the engine
//-------------------------------------------------------------------------
void FANCONTROL::SendCommand(int smart) {
	FCSHARED* shared = SharedState();
	if (!shared)
		return;

	::GetDlgItemText(this->hwndDialog, 8310, shared->cmdLevelText, sizeof(shared->cmdLevelText));

	shared->cmdSmart = smart;
	shared->cmdMode = this->CurrentModeFromDialog();

	this->LastCmdSeq = ::InterlockedIncrement(&shared->cmdSeq);
}

//-------------------------------------------------------------------------
//  create all named pipes for client GUI communication
//-------------------------------------------------------------------------
void FANCONTROL::CreateAllNamedPipes() {
	// TPFCIcon runs in the user's session, the engine usually as SYSTEM
	SECURITY_ATTRIBUTES sa;
	bool shared = SharedSecurity(&sa);

	for (int i = 0; i < NUM_PIPES; i++) {
		this->hPipes[i] = CreateNamedPipe(
			g_szPipeName,             // pipe name
			PIPE_ACCESS_OUTBOUND,     // write access
			PIPE_TYPE_MESSAGE |       // message type pipe
			PIPE_READMODE_MESSAGE |   // message-read mode
			PIPE_NOWAIT,              // blocking mode
			PIPE_UNLIMITED_INSTANCES, // max. instances
			BUFFER_SIZE,              // output buffer size
			BUFFER_SIZE,              // input buffer size
			NMPWAIT_USE_DEFAULT_WAIT, // client time-out
			shared ? &sa : NULL);     // readable from any session

		if (INVALID_HANDLE_VALUE == this->hPipes[i]) {
			// losing the icon pipe is not worth ending the program over
			char errBuf[128];
			sprintf_s(errBuf, sizeof(errBuf), "Creating Named Pipe %d client GUI was NOT successful, error %d.",
				i, ::GetLastError());
			this->Trace(errBuf);
		}
	}

	if (shared)
		::LocalFree(sa.lpSecurityDescriptor);
}

//-------------------------------------------------------------------------
//  write data to all named pipes
//-------------------------------------------------------------------------
void FANCONTROL::WriteAllNamedPipes(const char* data) {
	lbResult = bResult;
	for (int i = 0; i < NUM_PIPES; i++) {
		bResult = WriteFile(
			this->hPipes[i],          // handle to pipe
			data,                     // buffer to write from
			(DWORD)(strlen(data) + 1),// number of bytes to write, include the NULL
			&cbBytes,                 // number of bytes written
			NULL);                    // not overlapped I/O
	}
}

//-------------------------------------------------------------------------
//  close all named pipes
//-------------------------------------------------------------------------
void FANCONTROL::CloseAllNamedPipes() {
	for (int i = 0; i < NUM_PIPES; i++) {
		CloseHandle(this->hPipes[i]);
	}
}

//-------------------------------------------------------------------------
//  update the temperature sensor display list
//-------------------------------------------------------------------------
void
FANCONTROL::UpdateTempDisplay(void)
{
	HWND hLV = ::GetDlgItem(this->hwndDialog, 8101);
	if (!hLV) return;

	// Rebuilt every cycle: freeze painting and keep the scroll position, so the
	// list neither flickers nor jumps back to the top (Gorilla fork).
	int topIndex = ListView_GetTopIndex(hLV);
	::SendMessage(hLV, WM_SETREDRAW, FALSE, 0);
	ListView_DeleteAllItems(hLV);

	LVITEMA lvi = {0};
	lvi.mask = LVIF_TEXT;
	char buf[32];

	for (int i = 0; i < 12; i++)
	{
		int temp = this->State.Sensors[i];
		BOOL show = (temp < 128 && temp != 0) || (this->ShowAll == 1);

		if (show)
		{
			sprintf_s(buf, sizeof(buf), "%d", i + 1);
			lvi.iItem = i;
			lvi.iSubItem = 0;
			lvi.pszText = buf;
			int idx = ListView_InsertItem(hLV, &lvi);

			ListView_SetItemText(hLV, idx, 1, (LPSTR)this->State.SensorName[i]);

			if (temp < 128 && temp != 0)
			{
				if (Fahrenheit)
					sprintf_s(buf, sizeof(buf), "%d F", temp * 9 / 5 + 32);
				else
					sprintf_s(buf, sizeof(buf), "%d C", temp);
			}
			else
			{
				strcpy_s(buf, sizeof(buf), "n/a");
			}
			ListView_SetItemText(hLV, idx, 2, buf);
			sprintf_s(buf, sizeof(buf), "0x%02x", this->State.SensorAddr[i]);
			ListView_SetItemText(hLV, idx, 3, buf);
		}
	}

	int count = ListView_GetItemCount(hLV);
	if (topIndex > 0 && count > 0)
	{
		int last = topIndex + ListView_GetCountPerPage(hLV) - 1;
		ListView_EnsureVisible(hLV, last < count - 1 ? last : count - 1, FALSE);
	}
	::SendMessage(hLV, WM_SETREDRAW, TRUE, 0);
	::InvalidateRect(hLV, NULL, FALSE);

	this->icontemp = this->State.Sensors[this->iMaxTemp];

	// Invalidate header to update column 2 color based on MaxTemp
	HWND hHeader = ListView_GetHeader(hLV);
	if (hHeader) {
		InvalidateRect(hHeader, NULL, TRUE);
	}
}

//-------------------------------------------------------------------------
//  reading the EC status may take a while, hence do it in a thread
//-------------------------------------------------------------------------
int FANCONTROL::WorkThread() {
	int ok = this->ReadEcStatus(&this->State);

	::PostMessage(this->hwndDialog, WM__NEWDATA, ok, 0);

	return 0;
}

// The texticons will be shown depending on variables
static const int MAX_TEXT_ICONS = 16;
int icon, oldicon;
BOOL dishow(TRUE);
TCHAR myszTip[64];

void FANCONTROL::ProcessTextIcons(void) {
	oldicon = icon;
	if (this->CurrentModeFromDialog() == 1)
		icon = 10;    // gray
	else {
		icon = 11;    // blue
		for (int i = 0; i < ARRAYMAX(this->IconLevels); i++) {
			if (this->MaxTemp >= this->IconLevels[i])
				icon = 12 + i;    // yellow, orange, red
		}
	}

	if (this->IconColorFan) {
		switch (fan1speed / 1000) {
			case 0:
				break;
			case 1:
				icon = 21; //sehr hell grün
				break;
			case 2:
				icon = 22; //hell grün
				break;
			case 3:
				icon = 23; //grün
				break;
			case 4:
				icon = 24; //dunkel grün
				break;
			case 5:
				icon = 25; //sehr dunkel grün
				break;
			case 6:
				icon = 25; //sehr dunkel grün
				break;
			case 7:
				icon = 25; //sehr dunkel grün
				break;
			case 8:
				icon = 25; //sehr dunkel grün
				break;
			default:
				icon = oldicon;
				break;
		};
	}

	this->iFarbeIconB = icon;

	if (lstrcpyn(myszTip, this->Title2, sizeof(myszTip) - 1) == NULL) {
		myszTip[0] = '\0';
	}

	if (pTextIconMutex->Lock(100)) {
		//init ppTbTextIcon
		if (!ppTbTextIcon || this->TaskbarNew) {
			this->TaskbarNew = 0;
			ppTbTextIcon = new CTaskbarTextIcon * [MAX_TEXT_ICONS];
			for (int i = 0; i < MAX_TEXT_ICONS; ++i)
				ppTbTextIcon[i] = NULL;

			ppTbTextIcon[0] = new CTaskbarTextIcon(
				this->m_hinstapp,
				this->hwndDialog, WM__TASKBAR, 0, "", "",  //WM_APP+5000 -> WM__TASKBAR
				this->iFarbeIconB, this->iFontIconB, myszTip);

			if (dishow && !this->NoBallons) {
				if (Fahrenheit) {
					ppTbTextIcon[0]->DiShowballon(
						_T("shows max. temperature in ° F and sensor name, left click on icon shows or hides control window, right click shows menue"),
						_T("TPFanControl new text icon"), NIIF_INFO, 11);
				}
				else {
					ppTbTextIcon[0]->DiShowballon(
						_T("shows max. temperature in ° C and sensor name, left click on icon shows or hides control window, right click shows menue"),
						_T("TPFanControl new text icon"), NIIF_INFO, 11);
				}

				// Input:
				//  szText: [in] Text for the balloon tooltip.
				//  szTitle: [in] Title for the balloon.  This text is shown in bold above
				//           the tooltip text (szText).  Pass "" if you don't want a title.
				//  dwIcon: [in] Specifies an icon to appear in the balloon.  Legal values are:
				//                 NIIF_NONE: No icon
				//                 NIIF_INFO: Information
				//                 NIIF_WARNING: Exclamation
				//                 NIIF_ERROR: Critical error (red circle with X)
				//  uTimeout: [in] Number of seconds for the balloon to remain visible.  Can
				//            be between 10 and 30 inclusive.
				//

				dishow = FALSE;
			}
		}

		char str_value[256];
		for (int i = 0; i < MAX_TEXT_ICONS; ++i) {
			if (ppTbTextIcon[i]) {
				if (Fahrenheit)
					_itoa_s((this->icontemp * 9 / 5) + 32, str_value, sizeof(str_value), 10);
				else
					_itoa_s(this->icontemp, str_value, sizeof(str_value), 10);
				sprintf_s(str_value, sizeof(str_value), "%s", str_value);
				ppTbTextIcon[i]->ChangeText(str_value, this->gSensorNames[iMaxTemp], iFarbeIconB, iFontIconB, myszTip);
			}
		}
		pTextIconMutex->Unlock();
		//this->Trace(LastTooltip); 
	}
}

void FANCONTROL::RemoveTextIcons(void) {
	if (pTextIconMutex->Lock(10000)) {
		if (ppTbTextIcon) {
			for (int i = 0; i < MAX_TEXT_ICONS; ++i) {
				if (ppTbTextIcon[i]) {
					delete ppTbTextIcon[i];
				}
			}
			delete[] ppTbTextIcon;
			ppTbTextIcon = NULL;
		}
		pTextIconMutex->Unlock();
	}
	else {
		_ASSERT(false);//Mutex not av within 10 sec
	}
}
