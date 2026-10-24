#include "../src/intensity-firmware-gate.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    FCFirmwareGateContext ctx = {0, 0, 0};

    assert(fc_firmware_gate_decision(ctx, 0x0b) ==
           FC_FIRMWARE_VERSION_ACCEPTED);
    assert(fc_firmware_gate_decision(ctx, 0x0a) ==
           FC_FIRMWARE_VERSION_MISMATCH);
    assert(fc_firmware_gate_decision(ctx, 0xff) ==
           FC_FIRMWARE_VERSION_MISMATCH);

    ctx.guard_289 = 1;
    assert(fc_firmware_gate_decision(ctx, 0x0b) ==
           FC_FIRMWARE_INIT_GUARDS_CLOSED);

    ctx.guard_289 = 0;
    ctx.guard_28b = 1;
    assert(fc_firmware_gate_decision(ctx, 0x0b) ==
           FC_FIRMWARE_INIT_GUARDS_CLOSED);

    ctx.guard_28b = 0;
    ctx.skip_version_check_28c = 1;
    assert(fc_firmware_gate_decision(ctx, 0x00) ==
           FC_FIRMWARE_COMPARISON_SKIPPED);

    printf("stage=firmware_gate result=pass tests=6 hardware_access=0\n");
    return 0;
}
