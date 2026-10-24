#include "intensity-firmware-gate.h"

FCFirmwareGateDecision fc_firmware_gate_decision(
    FCFirmwareGateContext ctx, uint8_t reported_version)
{
    if (ctx.guard_289 != 0 || ctx.guard_28b != 0)
        return FC_FIRMWARE_INIT_GUARDS_CLOSED;
    if (ctx.skip_version_check_28c != 0)
        return FC_FIRMWARE_COMPARISON_SKIPPED;
    if (reported_version != FC_INTENSITY_A124_EXPECTED_FIRMWARE_VERSION)
        return FC_FIRMWARE_VERSION_MISMATCH;
    return FC_FIRMWARE_VERSION_ACCEPTED;
}
