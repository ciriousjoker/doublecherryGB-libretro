#pragma once
#include <cstdint>
class cpu;
// Optional host-only observation. Excluded from ordinary builds and all saves.
enum class CpuProfileKind : uint8_t { Instruction, MemoryWrite, Dma };
struct CpuProfileEvent {
    cpu *owner;
    CpuProfileKind kind;
    uint16_t pc, address;
    uint32_t rom_offset, ticks;
    uint8_t opcode;
    bool idle, double_speed;
};
using CpuProfileObserver = void (*)(const CpuProfileEvent &, void *);
#ifdef DCGB_CPU_PROFILE
void dcgb_set_cpu_observer(CpuProfileObserver observer, void *context);
#endif
