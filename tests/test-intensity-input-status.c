/*
 * Synthetic-only input status test. Never interacts with hardware.
 */
#include "../src/intensity-input-status.h"
#include <assert.h>
#include <stdio.h>

static FCInputStatusGates ready(void) {
    FCInputStatusGates g = {0, 1, 0, 8};
    return g;
}

static void run(void) {
    FCInputStatusGates g = ready();
    FCInputStatusResult r = fc_classify_input_status(g, 0x01u);
    assert(r.classification == FC_INPUT_BASIC_FORMAT_CANDIDATE);
    assert(r.basic_mode_index == 0);

    r = fc_classify_input_status(g, 0x01u | 0x02u | 0x800u);
    assert(r.classification == FC_INPUT_BASIC_FORMAT_CANDIDATE);
    assert(r.basic_mode_index == 3);

    r = fc_classify_input_status(g, 0x01u | 0x200u | (0x1eu << 12));
    assert(r.classification == FC_INPUT_MODE_TABLE_LOOKUP_REQUIRED);
    assert(r.format_family == 1 && r.format_code == 30);

    r = fc_classify_input_status(g, 0x01u | 0x400u | (0x1fu << 12));
    assert(r.classification == FC_INPUT_MODE_TABLE_LOOKUP_REQUIRED);
    assert(r.format_family == 2 && r.format_code == 31);

    r = fc_classify_input_status(g, 0x601u);
    assert(r.classification == FC_INPUT_UNSUPPORTED_STATUS_ENCODING);

    r = fc_classify_input_status(g, UINT32_MAX);
    assert(r.classification == FC_INPUT_UNREADABLE_REGISTER);

    r = fc_classify_input_status(g, 0x0u);
    assert(r.classification == FC_INPUT_NO_SIGNAL_FLAG);

    g.readiness_28a = 0;
    r = fc_classify_input_status(g, 0x01u);
    assert(r.classification == FC_INPUT_GATES_CLOSED);

    g = ready();
    g.guard_289 = 1;
    r = fc_classify_input_status(g, 0x01u);
    assert(r.classification == FC_INPUT_GATES_CLOSED);

    g = ready();
    g.guard_28b = 1;
    r = fc_classify_input_status(g, 0x01u);
    assert(r.classification == FC_INPUT_GATES_CLOSED);

    g = ready();
    g.feature_1eb = 0;
    r = fc_classify_input_status(g, 0x01u);
    assert(r.classification == FC_INPUT_GATES_CLOSED);
}

int main(void) {
    run();
    puts("stage=input_status_classifier result=pass tests=11 hardware_access=0");
    return 0;
}
