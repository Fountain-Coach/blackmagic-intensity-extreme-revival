/*
 * Pure, independently authored classifier for a limited HDExtreme status
 * encoding. This function cannot determine the actual connected HDMI source.
 */
#include "intensity-input-status.h"

FCInputStatusResult fc_classify_input_status(
    FCInputStatusGates gates, uint32_t raw_status)
{
    FCInputStatusResult result = {FC_INPUT_GATES_CLOSED, 0, 0, 0};

    /* Control-flow guards in the base detected-input-format method.
     * Deliberately avoid attributing undocumented semantic names to bytes.
     */
    if (gates.guard_289 != 0 || gates.readiness_28a == 0 ||
        gates.guard_28b != 0 || (gates.feature_1eb & 0x08u) == 0)
        return result;

    if (raw_status == UINT32_MAX) {
        result.classification = FC_INPUT_UNREADABLE_REGISTER;
        return result;
    }
    if ((raw_status & 0x01u) == 0) {
        result.classification = FC_INPUT_NO_SIGNAL_FLAG;
        return result;
    }

    switch (raw_status & 0x600u) {
    case 0:
        result.classification = FC_INPUT_BASIC_FORMAT_CANDIDATE;
        result.basic_mode_index = (uint8_t)(
            ((raw_status & 0x02u) ? 1u : 0u) +
            ((raw_status & 0x800u) ? 2u : 0u));
        break;
    case 0x200u:
    case 0x400u:
        /* Requires the vendor's 66-entry mode metadata; do NOT guess a mode. */
        result.classification = FC_INPUT_MODE_TABLE_LOOKUP_REQUIRED;
        result.format_family = (uint8_t)((raw_status & 0x600u) >> 9);
        result.format_code = (uint8_t)((raw_status >> 12) & 0x1fu);
        break;
    default:
        result.classification = FC_INPUT_UNSUPPORTED_STATUS_ENCODING;
        break;
    }
    return result;
}
