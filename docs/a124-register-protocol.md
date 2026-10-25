# A124 register and output-control evidence ledger

Updated: 25 October 2026.
Status: PARTIAL STATIC PROTOCOL. Not an independently operating kernel driver.
Target: original Intensity Extreme, PCI vendor BDBD and device A124, model code 0x36.

## Evidential limits

PHYSICAL OBSERVED means an actual read-only PCI/IORegistry/DeckLink status from Sierra. STATIC means a recovered instruction, vtable target or model table from the exact vendor driver held privately on Ubuntu. SYNTHETIC means our own test-provided register value. UNKNOWN means no authorized live operation or write may be inferred.

Exact original Desktop Video 10.4 driver SHA-256: c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780. The private binary is not copied into this repo. Source-owned inspectors verify the hash before accepting static findings.

| Component | Value or location | Provenance | What is NOT proved |
| --- | --- | --- | --- |
| Physical PCI identity | BDBD:A124 | PHYSICAL OBSERVED | Independent driver owns PCI |
| BAR0 descriptor | config offset 0x10, 1 MiB | PHYSICAL OBSERVED | Safe to read or write MMIO |
| BAR1 descriptor | config offset 0x14, 1 MiB | PHYSICAL OBSERVED | BAR1 meaning, DMA path |
| Input status | BAR0 + 0x40, 32-bit candidate | STATIC | Measured lock or a safe read |
| Firmware version | BAR0 + 0x224, low byte, expected 0x0B | STATIC | Actual live version or full readiness |
| HDMI OUT transmitter power/PHY enable | UNKNOWN | NOT RESOLVED | Electrical OFF or safe register write |
| HDMI hot-plug or sink status | UNKNOWN | STATIC EDID-related routines only | Panasonic EDID or electrical sink lock |
| Output clock/timing | UNKNOWN | STATIC mode operations | Correct TMDS output or pixel reception |
| Output DMA/IRQ | UNKNOWN | Partial STATIC DaisyCutter routines | Descriptor ownership and completion |
| IRQ acknowledgement | UNKNOWN | Partial STATIC paths | Safe interrupt clearing |

See [original input-status call chain](a124-input-status-call-chain-2026-10-24.md), [DMA evidence](dma-protocol-evidence-2026-10-23.md), and [HD Extreme obligations](hdextreme-protocol-obligations.md).

## Newly verified A124 output dispatch

The new [SHA-pinned static output verifier](../scripts/inspect_private_output_dispatch.py) parses the original Mach-O symbols and actual vtable pointer values. It does not execute vendor code.

1. HDExtreme SetVideoOutputMode has VM address 0x2d2ac.
2. Its inherited HDProPCIExpress raw-vtable call at offset 0x280 resolves to HDPro SetVideoOutputMode at 0x1846e.
3. HDPro SetVideoOutputMode itself invokes an inherited HD output mode routine, then uses device virtual dispatch. Its hardware effects still require detailed tracing.
4. HDExtreme mode follow-up through its object vtable address point plus offset 0x810 resolves to AutoSwitch3DPackingFormat at 0x32404.
5. HDExtreme ReadOutputEDID is present at 0x2ec88 and WriteHdmiSinkCapabilityBits at 0x2f3fa.
6. Static WriteHdmiSinkCapabilityBits contains readiness guards at object offsets 0x289/0x28a/0x28b, cached sink-capability fields, and indirect masked operations. None is yet identified as the output PHY OFF register.

Result from the locally held original SHA-verified binary:

    status=static_output_mode_dispatch_verified
    base_mode_target=0x1846e
    follow_up_target=0x32404
    hdmi_phy_off_register_resolved=false
    hdmi_tx_electrical_state_observed=false
    live_hardware_access=false

The [7 test cases](../tests/test_private_output_dispatch.py) passed 7/7 on Ubuntu with the exact private binary and 4/7 on modern macOS, where three original-binary-specific tests were correctly skipped. Modifying even one byte fails the hash gate.

These are function-call facts, NOT an identified transmitter-off register or an authorization to attempt MMIO writes.

## Why original 10.4 API status does not establish physical silence

Before shutdown, Sierra reported HDMI IN connector 0x2 and HDMI OUT connection mask 0xA (HDMI+component). The original vendor playback API has accepted 720p60 frames with a real device busy state, but the Panasonic's visible picture was never independently accepted. The owner subsequently reported repeated on/off cycling.

Setting output connection to component-only 0x8 returned nominal success but fresh readback remained 0xA. A preferences commit and idle DisableVideoOutput returned access denied. The vendor status GetBytes call is a nonfunctional stub rather than a Panasonic EDID reader; it caused a user-space fault when given a null result pointer. It is now prohibited by source-owned tests.

The emergency stop disabled two separate Fountain automatic launchers and left OUTPUT_DISABLED in place, the installed playback executable non-executable, and original vendor hardware idle. These are SOFTWARE STOP results only, not electrical HDMI OFF proof.

See [owner stop receipt](../tests/2026-10-25-panasonic-owner-emergency-stop.md).

## Independent software and offline driver milestones

The new [common safety interface](../driver/common/a124_safety.h) and [C implementation](../driver/common/a124_safety.c) have no I/O or runtime dependencies:
- Exact PCI identity and BAR sizes, with separate synthetic versus physical inventory provenance.
- Default-on owner stop latch, no unlock operation, no hardware transmitter arm routine.
- Hardware idle does not mean electrical HDMI OFF.
- Software interlock evidence never means physical OFF.
- A modeled receiver OFF requires two independent-receiver reports, monotonic timestamps, separated by at least 500 ms; subsequent cycling revokes acceptance. Future adapters must independently substantiate receiver provenance.
- Physical stop request fails closed while exclusive ownership or verified TX register contract is absent. No MMIO write callback exists.

All **48 C assertions passed on Sierra, modern macOS and Ubuntu**. No hardware access is included in these tests.

The existing [Sierra passive PCI diagnostic](../driver/sierra/passive/) now links the common C safety source and checks inventory/owner-stop before publishing properties. Its IOKitPersonalities is EMPTY, preventing automatic matching or competition with the vendor kernel driver.

Original silver Mac offline evidence:
- Xcode 9.2 x86_64 Mach-O KEXT compiled and linked successfully.
- kextutil -n -t -z returned 0 and reported the bundle appears loadable for offline linkage; it also reports the bundle is unsigned.
- Bundled Xcode 10.13 libkmod object files warned they are newer than the Sierra 10.12 target. Runtime compatibility remains unproven.
- Offline executable SHA-256 d485ff45e4be0f4576eb9c190035e0383bf9fc5f0794b370f55f6793ff834395.
- nm verifies fc_a124_safety_init, fc_a124_note_inventory and fc_a124_request_playback are linked.
- kextstat confirms original Blackmagic driver 10.4 still loaded, independent KEXT NOT loaded. Device still idle (0x0), software output stop retained.

## Hardware release gates

G0 software stop: PASS. Physical STOP: OPEN (Panasonic still reported cycling).
G1 protocol: PARTIAL STATIC, transmitter-off control UNKNOWN.
G2 independently owned software safety model: PASS as MODEL ONLY.
G3 offline nonmatching Sierra KEXT: PASS as OFFLINE ONLY, unsigned and not runtime-tested.
G4 isolated exclusive PCI recovery session: NOT YET WITNESSED.
G5 first independent BAR observation: NOT DONE.
G6 electrical HDMI OUT quiet and Panasonic stable: NOT DONE.
G7 genuine HDMI output with a stable Panasonic picture: NOT DONE.
G8 genuine captured HDMI input frames: NOT DONE.

Next engineering work is a narrower static trace of the output-mode, sink capability and electrical enable/disable call paths. DO NOT attempt guessed registers, install the passive KEXT, clear owner STOP, flash firmware, or launch any Panasonic playback to obtain the evidence.
