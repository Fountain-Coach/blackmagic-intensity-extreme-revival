/*
 * Portable fail-closed A124 protocol tests. No device access.
 * Build: cc -std=c99 -Wall -Wextra -Werror -pedantic
 *           -Idriver/common driver/common/a124_safety.c
 *           tests/test_a124_safety.c -o /tmp/test_a124_safety
 */
#include "a124_safety.h"
#include <stdio.h>
#include <string.h>

static int failed=0,passed=0;
#define REQUIRE(q) do {                                     \
    if(!(q)){                                                \
        fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#q); \
        failed++;                                            \
    }else{passed++;}                                        \
} while(0)

int main(void){
    FCA124Safety s;
    fc_a124_safety_init(&s);
    REQUIRE(s.owner_stop_latched==1);
    REQUIRE(s.exclusive_pci_owner_verified==0);
    REQUIRE(s.transmitter_off_contract_verified==0);
    REQUIRE(s.stop_evidence==FC_A124_STOP_UNKNOWN);
    REQUIRE(s.last_signal==FC_A124_SIGNAL_UNKNOWN);
    REQUIRE(fc_a124_request_playback(&s)==
            FC_A124_RESULT_DENIED_OWNER_STOP);
    REQUIRE(fc_a124_request_physical_stop(&s)==
            FC_A124_RESULT_DENIED_NO_EXCLUSIVE_OWNER);

    FCA124Inventory valid={
        FC_A124_VENDOR_ID,FC_A124_DEVICE_ID,
        FC_A124_EXPECTED_BAR_BYTES,FC_A124_EXPECTED_BAR_BYTES,
        FC_A124_SOURCE_SYNTHETIC
    };
    REQUIRE(fc_a124_note_inventory(&s,&valid)==
            FC_A124_RESULT_SOFTWARE_ONLY);
    REQUIRE(s.hardware_inventory_verified==0);
    valid.source=FC_A124_SOURCE_PCI_INVENTORY;
    REQUIRE(fc_a124_note_inventory(&s,&valid)==FC_A124_RESULT_ACCEPTED);
    REQUIRE(s.hardware_inventory_verified==1);
    valid.device_id=0;
    REQUIRE(fc_a124_note_inventory(&s,&valid)==
            FC_A124_RESULT_DENIED_BAD_INVENTORY);
    REQUIRE(s.hardware_inventory_verified==1);
    valid.device_id=FC_A124_DEVICE_ID;
    valid.bar1_bytes=0;
    REQUIRE(fc_a124_note_inventory(&s,&valid)==
            FC_A124_RESULT_DENIED_BAD_INVENTORY);
    valid.bar1_bytes=FC_A124_EXPECTED_BAR_BYTES;
    valid.source=FC_A124_SOURCE_DEVICE_STATUS;
    REQUIRE(fc_a124_note_inventory(&s,&valid)==
            FC_A124_RESULT_DENIED_PROVENANCE);
    REQUIRE(fc_a124_note_inventory(NULL,&valid)==
            FC_A124_RESULT_INVALID);

    REQUIRE(fc_a124_note_vendor_idle(&s,1)==
            FC_A124_RESULT_UNVERIFIED);
    REQUIRE(s.source_driver_reports_idle==1);
    REQUIRE(s.stop_evidence==FC_A124_STOP_UNKNOWN);
    REQUIRE(fc_a124_note_software_stop(&s,0)==
            FC_A124_RESULT_DENIED_OWNER_STOP);
    REQUIRE(fc_a124_note_software_stop(&s,1)==
            FC_A124_RESULT_SOFTWARE_ONLY);
    REQUIRE(s.stop_evidence==FC_A124_STOP_SOFTWARE_INTERLOCK);
    REQUIRE(fc_a124_request_playback(&s)==
            FC_A124_RESULT_DENIED_OWNER_STOP);

    FCA124OutputWitness witness={1000,FC_A124_SOURCE_SYNTHETIC,
                                   FC_A124_SIGNAL_OFF};
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_DENIED_PROVENANCE);
    REQUIRE(s.stop_evidence==FC_A124_STOP_SOFTWARE_INTERLOCK);
    witness.source=FC_A124_SOURCE_DEVICE_STATUS;
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_DENIED_PROVENANCE);
    witness.source=FC_A124_SOURCE_INDEPENDENT_RECEIVER;
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_UNVERIFIED);
    REQUIRE(s.stop_evidence!=FC_A124_STOP_PHYSICAL_OFF_WITNESSED);
    witness.tick_ms=1200;
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_UNVERIFIED);
    witness.tick_ms=1200;
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_DENIED_NON_MONOTONIC_WITNESS);
    witness.tick_ms=1800;
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_ACCEPTED);
    REQUIRE(s.stop_evidence==FC_A124_STOP_PHYSICAL_OFF_WITNESSED);
    witness.tick_ms=1900;
    witness.receiver_signal=FC_A124_SIGNAL_CYCLING;
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_UNVERIFIED);
    REQUIRE(s.stop_evidence==FC_A124_STOP_SOFTWARE_INTERLOCK);
    witness.tick_ms=2200;
    witness.receiver_signal=FC_A124_SIGNAL_OFF;
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_UNVERIFIED);
    witness.tick_ms=2700;
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_ACCEPTED);
    witness.tick_ms=2900;
    witness.receiver_signal=FC_A124_SIGNAL_STABLE_VIDEO;
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_UNVERIFIED);
    REQUIRE(s.stop_evidence==FC_A124_STOP_SOFTWARE_INTERLOCK);
    witness.tick_ms=3000;
    witness.receiver_signal=FC_A124_SIGNAL_UNKNOWN;
    REQUIRE(fc_a124_note_receiver(&s,witness)==
            FC_A124_RESULT_UNVERIFIED);
    REQUIRE(s.stop_evidence==FC_A124_STOP_SOFTWARE_INTERLOCK);
    REQUIRE(fc_a124_note_receiver(NULL,witness)==FC_A124_RESULT_INVALID);

    /* Even forced internal flag mutations cannot turn a model into an
     * independently running hardware driver: no register-write API exists.
     */
    s.owner_stop_latched=0;
    REQUIRE(fc_a124_request_playback(&s)==
            FC_A124_RESULT_DENIED_NO_EXCLUSIVE_OWNER);
    s.exclusive_pci_owner_verified=1;
    REQUIRE(fc_a124_request_physical_stop(&s)==
            FC_A124_RESULT_DENIED_NO_TX_REGISTER_CONTRACT);
    REQUIRE(fc_a124_request_playback(&s)==
            FC_A124_RESULT_DENIED_NO_TX_REGISTER_CONTRACT);
    s.transmitter_off_contract_verified=1;
    REQUIRE(fc_a124_request_physical_stop(&s)==
            FC_A124_RESULT_UNVERIFIED);
    REQUIRE(fc_a124_request_playback(&s)==
            FC_A124_RESULT_UNVERIFIED);
    REQUIRE(fc_a124_request_playback(NULL)==FC_A124_RESULT_INVALID);
    REQUIRE(fc_a124_request_physical_stop(NULL)==
            FC_A124_RESULT_INVALID);

    printf("stage=a124_safety_contract result=%s assertions_passed=%d"
           " assertions_failed=%d original_output_writes=0"
           " hardware_access=0 optical_evidence=synthetic_only\n",
           failed?"FAIL":"PASS",passed,failed);
    return failed?1:0;
}
