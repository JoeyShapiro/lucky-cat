#include <intrin.h>

unsigned __int64 read_msr(unsigned long register_id) {
    //unsigned long high, low;
    //__readmsr(register_id, &low, &high);
    //return ((unsigned __int64)high << 32) | low;

    return __readmsr(register_id);
}

double get_cpu_freq(void) {
    // Read base frequency from MSR_PLATFORM_INFO
    unsigned __int64 platform_info = read_msr(0xCE);
    unsigned long base_freq = (platform_info >> 8) & 0xFF; // Extract bits 8-15

    // Read current multiplier from IA32_PERF_STATUS
    unsigned __int64 perf_status = read_msr(0x198);
    unsigned long curr_multiplier = (perf_status >> 8) & 0xFF;

    // Frequency in MHz = base_freq * current_multiplier
    return base_freq * curr_multiplier;
}
