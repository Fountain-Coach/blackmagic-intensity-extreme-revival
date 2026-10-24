/*
 * Fountain Coach - independent HDExtreme input-status classifier.
 *
 * Evidence: Desktop Video 10.4, A124 model code 0x36, base
 * VideoInputGetDetectedInputFormat static control-flow reconstruction.
 * This is a PURE function. It neither reads MMIO nor accesses Blackmagic API.
 * It does not claim live signal lock or decode complete display modes.
 *
 * Copyright (c) 2026 Fountain Coach contributors. MIT.
 */
#ifndef FC_INTENSITY_INPUT_STATUS_H
#define FC_INTENSITY_INPUT_STATUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t guard_289;
    uint8_t readiness_28a;
    uint8_t guard_28b;
    uint8_t feature_1eb;
} FCInputStatusGates;

typedef enum {
    FC_INPUT_GATES_CLOSED = 0,
    FC_INPUT_UNREADABLE_REGISTER = 1,
    FC_INPUT_NO_SIGNAL_FLAG = 2,
    FC_INPUT_BASIC_FORMAT_CANDIDATE = 3,
    FC_INPUT_MODE_TABLE_LOOKUP_REQUIRED = 4,
    FC_INPUT_UNSUPPORTED_STATUS_ENCODING = 5
} FCInputStatusClassification;

typedef struct {
    FCInputStatusClassification classification;
    uint8_t basic_mode_index;
    uint8_t format_family;
    uint8_t format_code;
} FCInputStatusResult;

/*
 * Analyzes caller-supplied SYNTHETIC or separately measured status.
 * In particular, raw_status is NOT obtained or read from BAR0 here.
 * The readiness bytes represent private object-state checks reconstructed
 * from binary; they are NOT values of public I/O Registry properties.
 */
FCInputStatusResult fc_classify_input_status(
    FCInputStatusGates gates, uint32_t raw_status);

#ifdef __cplusplus
}
#endif
#endif
