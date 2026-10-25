#include "a124_safety.h"
#include <stddef.h>
#include <string.h>

void fc_a124_safety_init(FCA124Safety *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->owner_stop_latched = 1;
    s->stop_evidence = FC_A124_STOP_UNKNOWN;
    s->last_signal = FC_A124_SIGNAL_UNKNOWN;
}

FCA124Result fc_a124_note_inventory(FCA124Safety *s,
                                    const FCA124Inventory *inventory)
{
    if (!s || !inventory) return FC_A124_RESULT_INVALID;
    if (inventory->vendor_id != FC_A124_VENDOR_ID ||
        inventory->device_id != FC_A124_DEVICE_ID ||
        inventory->bar0_bytes != FC_A124_EXPECTED_BAR_BYTES ||
        inventory->bar1_bytes != FC_A124_EXPECTED_BAR_BYTES)
        return FC_A124_RESULT_DENIED_BAD_INVENTORY;

    /* Copying a fixture is not a physical PCI witness. */
    if (inventory->source != FC_A124_SOURCE_PCI_INVENTORY &&
        inventory->source != FC_A124_SOURCE_SYNTHETIC)
        return FC_A124_RESULT_DENIED_PROVENANCE;
    s->inventory = *inventory;
    s->hardware_inventory_verified =
        (uint8_t)(inventory->source == FC_A124_SOURCE_PCI_INVENTORY);
    return s->hardware_inventory_verified
        ? FC_A124_RESULT_ACCEPTED : FC_A124_RESULT_SOFTWARE_ONLY;
}

FCA124Result fc_a124_note_vendor_idle(FCA124Safety *s, int idle)
{
    if (!s) return FC_A124_RESULT_INVALID;
    s->source_driver_reports_idle = (uint8_t)(idle != 0);
    /* Important: NO modification of physical stop evidence. */
    return FC_A124_RESULT_UNVERIFIED;
}

FCA124Result fc_a124_note_software_stop(FCA124Safety *s, int lock_active)
{
    if (!s) return FC_A124_RESULT_INVALID;
    if (!lock_active) return FC_A124_RESULT_DENIED_OWNER_STOP;
    s->owner_stop_latched = 1;
    if (s->stop_evidence == FC_A124_STOP_UNKNOWN)
        s->stop_evidence = FC_A124_STOP_SOFTWARE_INTERLOCK;
    return FC_A124_RESULT_SOFTWARE_ONLY;
}

FCA124Result fc_a124_note_receiver(FCA124Safety *s,
                                   FCA124OutputWitness witness)
{
    if (!s) return FC_A124_RESULT_INVALID;
    if (witness.source != FC_A124_SOURCE_INDEPENDENT_RECEIVER)
        return FC_A124_RESULT_DENIED_PROVENANCE;
    if (witness.receiver_signal == FC_A124_SIGNAL_UNKNOWN)
        return FC_A124_RESULT_UNVERIFIED;
    if (witness.tick_ms == 0 || witness.tick_ms <= s->last_witness_tick_ms)
        return FC_A124_RESULT_DENIED_NON_MONOTONIC_WITNESS;

    const uint64_t delta = witness.tick_ms - s->last_witness_tick_ms;
    if (witness.receiver_signal == FC_A124_SIGNAL_OFF) {
        if (s->last_signal == FC_A124_SIGNAL_OFF && delta >= 500)
            s->consecutive_off_samples = 2;
        else
            s->consecutive_off_samples = 1;
    } else {
        s->consecutive_off_samples = 0;
        /* A new independent receiver observation reverses earlier
         * physical-off acceptance. It must never silently persist. */
        s->stop_evidence = FC_A124_STOP_SOFTWARE_INTERLOCK;
    }
    s->last_signal = witness.receiver_signal;
    s->last_witness_tick_ms = witness.tick_ms;

    if (s->consecutive_off_samples >= 2) {
        s->stop_evidence = FC_A124_STOP_PHYSICAL_OFF_WITNESSED;
        return FC_A124_RESULT_ACCEPTED;
    }
    return FC_A124_RESULT_UNVERIFIED;
}

FCA124Result fc_a124_request_playback(const FCA124Safety *s)
{
    if (!s) return FC_A124_RESULT_INVALID;
    if (s->owner_stop_latched)
        return FC_A124_RESULT_DENIED_OWNER_STOP;
    if (!s->exclusive_pci_owner_verified)
        return FC_A124_RESULT_DENIED_NO_EXCLUSIVE_OWNER;
    if (!s->transmitter_off_contract_verified)
        return FC_A124_RESULT_DENIED_NO_TX_REGISTER_CONTRACT;
    /* No code in this module arms actual hardware, even if called with
     * caller-modified state. An independently authorized owner/session
     * interface has not been designed or implemented. */
    return FC_A124_RESULT_UNVERIFIED;
}

FCA124Result fc_a124_request_physical_stop(const FCA124Safety *s)
{
    if (!s) return FC_A124_RESULT_INVALID;
    if (!s->exclusive_pci_owner_verified)
        return FC_A124_RESULT_DENIED_NO_EXCLUSIVE_OWNER;
    if (!s->transmitter_off_contract_verified)
        return FC_A124_RESULT_DENIED_NO_TX_REGISTER_CONTRACT;
    /* The actual OFF register address and write semantics are UNKNOWN.
     * No MMIO callback is offered; never fabricate a successful stop. */
    return FC_A124_RESULT_UNVERIFIED;
}
