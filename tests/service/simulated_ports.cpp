// User-mode test DLL for the production TVicPort adapter. No device or kernel
// driver is opened. The service runs against this DLL only in a disposable VM.
#include <windows.h>
#include <cstdio>

static unsigned char command, address, level = 0x80;
static bool needAddress, ready;
static int cpu = 40, stalled = 0, rejectFixed = 0, rejectBios = 0;
static unsigned readyPolls;

static void loadSample() {
    FILE* f = nullptr;
    if (fopen_s(&f, "scenario.txt", "r") == 0) {
        int t, stall, fixed, bios;
        if (fscanf_s(f, "%d%d%d%d", &t, &stall, &fixed, &bios) == 4) {
            cpu = t; stalled = stall; rejectFixed = fixed; rejectBios = bios;
        }
        fclose(f);
    }
}

static void record(unsigned requested, unsigned observed) {
    FILE* f = nullptr;
    if (fopen_s(&f, "register-writes.csv", "a") == 0) {
        fprintf(f, "%llu,0x%02x,0x%02x,0x%02x,%d\n",
                GetTickCount64(), address, requested, observed, cpu);
        fclose(f);
    }
}

extern "C" BOOL __stdcall OpenTVicPort() { return TRUE; }
extern "C" void __stdcall CloseTVicPort() {}
extern "C" unsigned char __stdcall ReadPort(unsigned short port) {
    if (port == 0x66) {
        if (ready && readyPolls) { --readyPolls; return 0; }
        return ready ? 1 : 0;
    }
    if (port != 0x62 || !ready) return 0xff;
    ready = false;
    static const int speeds[] = {0,1765,2183,2588,2788,2986,3182,3488};
    int rpm = stalled ? 0 : level <= 7 ? speeds[level] : level == 64 ? 4518 : 2400;
    switch (address) {
    case 0x2f: loadSample(); return level;
    case 0x78: return static_cast<unsigned char>(cpu);
    case 0x84: return static_cast<unsigned char>(rpm);
    case 0x85: return static_cast<unsigned char>(rpm >> 8);
    default: return 0;
    }
}

extern "C" void __stdcall WritePort(unsigned short port, unsigned char byte) {
    if (port == 0x66) { command = byte; needAddress = true; return; }
    if (port != 0x62) return;
    if (needAddress) {
        address = byte; needAddress = false;
        ready = command == 0x80;
        readyPolls = 1;
    } else if (command == 0x81) {
        if (address == 0x2f) {
            loadSample();
            if (!(byte == 128 ? rejectBios : rejectFixed)) level = byte;
        }
        record(byte, level);
    }
}
