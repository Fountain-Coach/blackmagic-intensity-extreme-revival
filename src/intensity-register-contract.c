#include "intensity-register-contract.h"

FCReadContractResult fc_evaluate_a124_read_contract(
    FCRegisterRead32 read32, void* context,
    FCFirmwareGateContext firmware_gates,
    FCInputStatusGates input_gates)
{
    FCReadContractResult r = {
        FC_READ_CONTRACT_INVALID, FC_FIRMWARE_INIT_GUARDS_CLOSED,
        {FC_INPUT_GATES_CLOSED, 0, 0, 0}, 0, 0, 0
    };
    r.state = FC_READ_CONTRACT_INVALID;
    if (!read32)
        return r;

    if (firmware_gates.guard_289 || firmware_gates.guard_28b) {
        r.state = FC_READ_CONTRACT_GUARDS_CLOSED;
        r.firmware = FC_FIRMWARE_INIT_GUARDS_CLOSED;
        return r;
    }

    uint32_t version_word = 0;
    if (!firmware_gates.skip_version_check_28c) {
        ++r.register_reads;
        if (!read32(context, FC_A124_VERSION_BAR0_OFFSET, &version_word) ||
            version_word == UINT32_MAX) {
            r.state = FC_READ_CONTRACT_VERSION_UNREADABLE;
            return r;
        }
    }

    r.reported_version = (uint8_t)(version_word & FC_A124_VERSION_MASK);
    r.firmware = fc_firmware_gate_decision(firmware_gates, r.reported_version);
    if (r.firmware != FC_FIRMWARE_VERSION_ACCEPTED &&
        r.firmware != FC_FIRMWARE_COMPARISON_SKIPPED) {
        r.state = FC_READ_CONTRACT_VERSION_REJECTED;
        return r;
    }

    /* This is only a MODEL of internal readiness established by the original
     * driver. It does NOT assert the physical firmware is ready or modify it.
     */
    input_gates.readiness_28a = 1;
    if (input_gates.guard_289 || input_gates.guard_28b ||
        (input_gates.feature_1eb & 0x08u) == 0u) {
        r.input.classification = FC_INPUT_GATES_CLOSED;
        r.state = FC_READ_CONTRACT_GUARDS_CLOSED;
        return r;
    }
    ++r.register_reads;
    if (!read32(context, FC_A124_INPUT_STATUS_BAR0_OFFSET,
                &r.raw_input_status)) {
        r.state = FC_READ_CONTRACT_STATUS_UNREADABLE;
        return r;
    }
    r.input = fc_classify_input_status(input_gates, r.raw_input_status);
    r.state = r.input.classification == FC_INPUT_UNREADABLE_REGISTER
                  ? FC_READ_CONTRACT_STATUS_UNREADABLE
                  : FC_READ_CONTRACT_STATUS_INTERPRETED;
    return r;
}
