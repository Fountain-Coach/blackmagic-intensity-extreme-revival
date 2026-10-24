/*
 * HDExtreme A124 firmware-version acceptance gate (pure model).
 * No device access, firmware upload, proprietary runtime or MMIO operations.
 * The result is NOT a determination that a live device is initialized.
 *
 * Copyright (c) 2026 Fountain Coach contributors. MIT.
 */
#ifndef FC_INTENSITY_FIRMWARE_GATE_H
#define FC_INTENSITY_FIRMWARE_GATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { FC_INTENSITY_A124_EXPECTED_FIRMWARE_VERSION = 0x0B };

typedef struct {
    uint8_t guard_289;
    uint8_t guard_28b;
    uint8_t skip_version_check_28c;
} FCFirmwareGateContext;

typedef enum {
    FC_FIRMWARE_INIT_GUARDS_CLOSED = 0,
    FC_FIRMWARE_VERSION_MISMATCH = 1,
    FC_FIRMWARE_VERSION_ACCEPTED = 2,
    FC_FIRMWARE_COMPARISON_SKIPPED = 3
} FCFirmwareGateDecision;

/* Version is the previously acquired/validated low byte, not an MMIO pointer.
 * The origin of the value MUST be separately established.
 * ACCEPTED means only that this isolated gate accepts the value.
 */
FCFirmwareGateDecision fc_firmware_gate_decision(
    FCFirmwareGateContext ctx, uint8_t reported_version);

#ifdef __cplusplus
}
#endif
#endif
