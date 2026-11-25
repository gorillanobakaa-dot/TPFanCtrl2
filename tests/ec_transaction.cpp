#include "../fancontrol/ectransaction.h"
#include "../fancontrol/fanpolicy.h"
#include <cassert>
#include <cstdio>
#include <vector>

struct FakePorts {
    std::vector<uint8_t> status;
    std::vector<uint8_t> writes;
    unsigned next = 0, dataReads = 0, calls = 0, failAt = 0;
    uint64_t clock = 0;
    uint64_t now() const { return clock; }
    void pause() { ++clock; }
    bool read(uint16_t port, uint8_t& out) {
        if (++calls == failAt) return false;
        if (port == 0x66) {
            assert(!status.empty());
            out = next < status.size() ? status[next++] : status.back();
        } else {
            assert(port == 0x62);
            ++dataReads;
            out = 0x42;
        }
        return true;
    }
    bool write(uint16_t port, uint8_t byte) {
        if (++calls == failAt) return false;
        assert(port == (writes.empty() ? 0x66 : 0x62));
        writes.push_back(byte);
        return true;
    }
};

int main() {
    assert(!FanReadyForFull(7, 0, 3488));
    assert(!FanReadyForFull(7, 1500, 3488));
    assert(FanReadyForFull(7, 3450, 3488));
    for (int target : {40, 45, 50}) {
        for (int floor : {1, 2, 3, 4, 5, 6, 7, 64}) {
            assert(GorillaFanLevel(target-7, target, floor, 7, false, false) == floor);
            assert(GorillaFanLevel(target-5, target, floor, 2, false, false) == (floor == 64 ? 64 : 7));
            assert(GorillaFanLevel(target-1, target, floor, 64, false, false) == 64);
            assert(GorillaFanLevel(target, target, floor, 2, false, false) == 64);
            assert(GorillaFanLevel(20, target, floor, 2, true, false) == 64);
            assert(GorillaFanLevel(20, target, floor, 2, false, true) == 64);
        }
    }
    assert(CompileFanDemand(49,70,false,false,false,false,0).level == 4);
    assert(CompileFanDemand(50,70,false,false,false,false,0).level == 5);
    assert(CompileFanDemand(40,70,false,false,false,true,1).level == 5);
    assert(CompileFanDemand(40,70,false,false,false,true,2).level == 7);
    assert(CompileFanDemand(60,70,false,false,false,false,0).level == 7);
    assert(CompileFanDemand(70,70,false,false,false,false,0).level == 64);
    assert(CompileFanDemand(40,70,false,true,false,false,0).level == 64);
    assert(CompileFanDemand(40,70,false,false,true,false,0).level == 64);

    assert(DecodePackageTemperature(100ULL << 16, (1ULL << 31) | (10ULL << 16)) == 90);
    assert(DecodePackageTemperature(100ULL << 16, 10ULL << 16) == -1);
    assert(DecodePackageTemperature(0, 1ULL << 31) == -1);
    assert(CoolingTemperature(66, -1, 100, 103, 104) == -1);
    assert(CoolingTemperature(66, 70, 100, 103, 5101) == -1);
    assert(CoolingTemperature(66, 70, 6000, 103, 6001) == -1);
    // A fresh, steady sample, idle load, no hysteresis, fan already at regulated 7.
    struct Case { int ec, package, expected; };
    for (auto sample : {Case{66,66,7}, Case{66,70,64}, Case{70,66,64},
                        Case{66,71,64}, Case{86,90,64}, Case{90,86,64}}) {
        int t = CoolingTemperature(sample.ec, sample.package, 100, 103, 104);
        auto demand = CompileFanDemand(t, 70, t >= 78, false, false, false, 0);
        int command = demand.level == 64 && !FanReadyForFull(7, 3488, 3488) ? 7 : demand.level;
        std::printf("sim ec=%d ec_ms=100 package=%d package_ms=103 steer=%d cmd=%d reason=%s\n",
                    sample.ec, sample.package, t, command, demand.reason);
        assert(command == sample.expected);
    }
    uint8_t value = 0;
    FakePorts delayed{{0, 2, 0, 0, 0, 1}};
    assert(TransferEcByte(delayed, 0x66, 0x62, 0x2f, value, false, 10));
    assert(value == 0x42 && delayed.dataReads == 1 && delayed.clock == 2);
    assert((delayed.writes == std::vector<uint8_t>{0x80, 0x2f}));

    FakePorts foreignReply{{1}};
    assert(!TransferEcByte(foreignReply, 0x66, 0x62, 0x2f, value, true, 10));
    assert(foreignReply.dataReads == 0 && foreignReply.writes.empty());

    FakePorts late{{0}};
    assert(!TransferEcByte(late, 0x66, 0x62, 0x2f, value, false, 10));
    assert(late.dataReads == 0 && late.clock == 10);

    for (bool write : {false, true}) {
        FakePorts good{write ? std::vector<uint8_t>{0} : std::vector<uint8_t>{0,0,0,1}};
        value = 7;
        assert(TransferEcByte(good, 0x66, 0x62, 0x2f, value, write, 10));
        if (write) assert((good.writes == std::vector<uint8_t>{0x81, 0x2f, 7}));
        for (unsigned op = 1; op <= good.calls; ++op) {
            FakePorts failed{good.status};
            failed.failAt = op;
            value = 7;
            assert(!TransferEcByte(failed, 0x66, 0x62, 0x2f, value, write, 10));
            assert(failed.calls == op && value == 7);
        }
    }
}
