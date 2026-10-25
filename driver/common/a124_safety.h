/*
 * Fountain Coach - independently owned A124 PCIe output safety contract.
 * Portable C99; no OS headers, DeckLink runtime, MMIO, DMA, IRQ or firmware.
 *
 * The current owner's STOP is latched by default. No code in this module
 * unlocks it, enables the transmitter, or claims a live HDMI result.
 * A physical STOP needs independent receiver evidence; neither vendor
 * API success nor "device busy = 0" qualifies.
 *
 * Copyright 2026 Fountain Coach contributors. MIT.
 */
#ifndef FOUNTAIN_A124_SAFETY_H
#define FOUNTAIN_A124_SAFETY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum {
    FC_A124_VENDOR_ID = 0xBDBD,
    FC_A124_DEVICE_ID = 0xA124,
    FC_A124_EXPECTED_BAR_BYTES = 1024 * 1024
};

typedef enum {
    FC_A124_SOURCE_UNSPECIFIED = 0,
    FC_A124_SOURCE_SYNTHETIC = 1,
    FC_A124_SOURCE_PCI_INVENTORY = 2,
    FC_A124_SOURCE_DEVICE_STATUS = 3,
    FC_A124_SOURCE_INDEPENDENT_RECEIVER = 4
} FCA124Source;

typedef enum {
    FC_A124_SIGNAL_UNKNOWN = 0,
    FC_A124_SIGNAL_CYCLING = 1,
    FC_A124_SIGNAL_OFF = 2,
    FC_A124_SIGNAL_STABLE_VIDEO = 3
} FCA124ReceiverSignal;

typedef enum {
    FC_A124_STOP_UNKNOWN = 0,
    FC_A124_STOP_SOFTWARE_INTERLOCK = 1,
    FC_A124_STOP_PHYSICAL_OFF_WITNESSED = 2
} FCA124StopEvidence;

typedef enum {
    FC_A124_RESULT_INVALID = 0,
    FC_A124_RESULT_ACCEPTED = 1,
    FC_A124_RESULT_SOFTWARE_ONLY = 2,
    FC_A124_RESULT_UNVERIFIED = 3,
    FC_A124_RESULT_DENIED_OWNER_STOP = 4,
    FC_A124_RESULT_DENIED_NO_EXCLUSIVE_OWNER = 5,
    FC_A124_RESULT_DENIED_NO_TX_REGISTER_CONTRACT = 6,
    FC_A124_RESULT_DENIED_BAD_INVENTORY = 7,
    FC_A124_RESULT_DENIED_PROVENANCE = 8,
    FC_A124_RESULT_DENIED_NON_MONOTONIC_WITNESS = 9
} FCA124Result;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t bar0_bytes;
    uint32_t bar1_bytes;
    FCA124Source source;
} FCA124Inventory;

typedef struct {
    uint64_t tick_ms;
    FCA124Source source;
    FCA124ReceiverSignal receiver_signal;
} FCA124OutputWitness;

typedef struct {
    FCA124Inventory inventory;
    uint8_t owner_stop_latched;
    uint8_t hardware_inventory_verified;
    uint8_t exclusive_pci_owner_verified;
    uint8_t transmitter_off_contract_verified;
    uint8_t source_driver_reports_idle;
    FCA124StopEvidence stop_evidence;
    FCA124ReceiverSignal last_signal;
    uint64_t last_witness_tick_ms;
    uint8_t consecutive_off_samples;
} FCA124Safety;

/* Always initializes with STOP latched, physical TX unverified. */
void fc_a124_safety_init(FCA124Safety *s);

/* Exact model/BAR geometry only. Synthetic inventory cannot prove hardware. */
FCA124Result fc_a124_note_inventory(FCA124Safety *s,
                                     const FCA124Inventory *inventory);

/* Reporting vendor busy=0 never proves HDMI electrical silence. */
FCA124Result fc_a124_note_vendor_idle(FCA124Safety *s, int idle);

/* Acknowledges owner software interlock, not the physical HDMI waveform. */
FCA124Result fc_a124_note_software_stop(FCA124Safety *s, int lock_active);

/* Independent receiver measurements must be ordered, provenance-typed. 
 * Two OFF samples at least 500 ms apart can produce PHYSICAL_OFF_WITNESSED.
 * The future adapter MUST substantiate INDEPENDENT_RECEIVER provenance.
 * This is evidence bookkeeping only, not a device command.
 */
FCA124Result fc_a124_note_receiver(FCA124Safety *s,
                                   FCA124OutputWitness witness);

/* Fail-closed command admission for original owner's STOP.
 * No release/override API: a future owner-authorized session needs a
 * separately reviewed gate. No hardware calls take place here.
 */
FCA124Result fc_a124_request_playback(const FCA124Safety *s);

/* A hardware stop write cannot be attempted until the PCI owner and
 * confirmed, model-specific transmitter register contract both exist.
 * This initial implementation deliberately has no register-write API.
 */
FCA124Result fc_a124_request_physical_stop(const FCA124Safety *s);

#ifdef __cplusplus
}
#endif
#endif
