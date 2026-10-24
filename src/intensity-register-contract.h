/*
 * Fountain Coach - independently authored, read-only A124 register contract.
 *
 * This API accepts an INJECTED read32 callback. There is no live MMIO,
 * firmware upload, kernel interaction, device ownership or write callback.
 * Nothing in the public source authorizes binding it to a physical BAR.
 */
#ifndef FC_INTENSITY_REGISTER_CONTRACT_H
#define FC_INTENSITY_REGISTER_CONTRACT_H

#include <stdint.h>
#include "intensity-firmware-gate.h"
#include "intensity-input-status.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FC_A124_INPUT_STATUS_BAR0_OFFSET = 0x40,
    FC_A124_VERSION_BAR0_OFFSET = 0x224,
    FC_A124_VERSION_MASK = 0xFF
};

typedef int (*FCRegisterRead32)(void* context, uint32_t bar0_offset,
                                uint32_t* value);

typedef enum {
    FC_READ_CONTRACT_INVALID = 0,
    FC_READ_CONTRACT_GUARDS_CLOSED = 1,
    FC_READ_CONTRACT_VERSION_UNREADABLE = 2,
    FC_READ_CONTRACT_VERSION_REJECTED = 3,
    FC_READ_CONTRACT_STATUS_UNREADABLE = 4,
    FC_READ_CONTRACT_STATUS_INTERPRETED = 5
} FCReadContractState;

typedef struct {
    FCReadContractState state;
    FCFirmwareGateDecision firmware;
    FCInputStatusResult input;
    uint8_t reported_version;
    uint32_t raw_input_status;
    uint32_t register_reads;
} FCReadContractResult;

/*
 * Model-only staged diagnostic:
 * - does not claim hardware initialization, HDMI lock or source validity;
 * - never maps/registers/acquires a device or writes to it;
 * - reports only gate outcomes using a supplied callback.
 *
 * The caller is responsible for establishing whether read32 is authorized.
 * For all current tests it MUST be a synthetic/mock source.
 */
FCReadContractResult fc_evaluate_a124_read_contract(
    FCRegisterRead32 read32, void* context,
    FCFirmwareGateContext firmware_gates,
    FCInputStatusGates input_gates);

#ifdef __cplusplus
}
#endif
#endif
