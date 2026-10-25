# Independent A124 driver G0–G3 implementation acceptance

Date: 25 October 2026. Owner: Fountain Coach.
Exact hardware: original silver Macmini5,3, Sierra 10.12.6, original A124 Intensity Extreme over original Thunderbolt + Comdis dock, original Blackmagic Desktop Video 10.4 loaded.
Separate research/test environment: existing Ubuntu Macmini8,1 and modern macOS laptop/desktop. The A124 is NOT attached to Ubuntu.

## Goal and permissions

Continue the owner-approved independent driver plan without restarting the Panasonic output, unplugging HDMI, enabling unproved registers, replacing the vendor kernel driver or conducting any firmware operation. The original owner STOP is not revoked by the instruction to proceed with development. Physical Panasonic on/off is still an unresolved owner-observed defect.

## G0 read-only real hardware and software stop witness

Direct remote shell read-only inspection on the actual silver Mac:

    output lock PRESENT
    playback runnable NO
    disabled launch plist PRESENT
    disabled show app PRESENT
    launch jobs NONE
    playback processes NONE
    stage=device_model status=ok model=Intensity Extreme
    stage=attribute name=device_busy_state status=0x00000000 value=0x0
    vendor driver com.blackmagic-design.desktopvideo.iokit.driver (10.4)

These are **software stop** and device idle witnesses. They DO NOT establish that HDMI OUT electrically stopped sending toggling signals. The Panasonic was not viewed remotely. Owner reports it is switching on/off when attached.

## G1 newly SHA-pinned original hardware-output dispatch

Original proprietary binary SHA-256, held privately on Ubuntu:
c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780.

The [source-owned static output verifier](../scripts/inspect_private_output_dispatch.py) parses a real Mach-O vtable and rejects any unknown binary digest. It found:

    status=static_output_mode_dispatch_verified
    mode_entry=0x2d2ac
    root_mode_target=0x103ce
    base_mode_target=0x1846e
    conversion_bits_target=0x32140
    follow_up_target=0x32404
    edid_reader=0x2ec88
    sink_capabilities_writer=0x2f3fa
    hdmi_phy_off_register_resolved=false
    hdmi_tx_electrical_state_observed=false
    live_hardware_access=false

The inherited and model-specific call path was also independently cross-checked against the real original LLVM Mach-O disassembly on Ubuntu. **This is static control flow**, not proof of what the HDMI connector transmits.

The [static verifier test](test_private_output_dispatch.py) passes **7/7** on Ubuntu with the exact authorized original binary, including mutation rejection. Four generic tests passed on modern macOS; three private-original tests were correctly skipped. No proprietary driver binaries or disassembly dumps entered Git.

[Detailed output and register ledger](../docs/a124-register-protocol.md).

## G2 independently authored common safety contract

Committed:
- [header](../driver/common/a124_safety.h)
- [implementation](../driver/common/a124_safety.c)
- [48-assertion tests](test_a124_safety.c)

The code is portable C99, with **no actual hardware I/O**. It validates A124 PCI identity/BAR lengths, distinguishes synthetic inventory from physical inventory, keeps owner STOP latched by default without an unlock API, rejects playback and unproven physical stop requests, and distinguishes software interlock from independent receiver evidence. It never calls DeckLink, Syphon, PCI MMIO, firmware, DMA or IRQ.

Native tested output on original Sierra, modern macOS, and Ubuntu:

    stage=a124_safety_contract result=PASS assertions_passed=48 assertions_failed=0 original_output_writes=0 hardware_access=0 optical_evidence=synthetic_only

**Exact caveat:** receiver-origin values are provided by the caller; the future physical adapter must authenticate/witness the real receiver provenance. Two modeled receiver OFF reports are not themselves physical Panasonic proof.

## G3 offline-only original Sierra kernel extension

Modified pre-existing, deliberately unbound [passive Sierra PCI diagnostic](../driver/sierra/passive/FCIntensityPCIDiagnostic.cpp) to include the shared C safety core, verify modeled device identity and resource geometry, and explicitly publish that software STOP is latched while physical HDMI OUT OFF is unproved. The [offline build script](../driver/sierra/passive/build-passive.sh) now compiles and links this shared C object.

Tested on exact original Sierra Xcode 9.2, macOS 10.13 SDK targeting macOS 10.12:

    FCIntensityPCIDiagnostic.kext/Contents/Info.plist: OK
    .../FCIntensityPCIDiagnostic: Mach-O 64-bit kext bundle x86_64
    stage=passive_bundle_built
    note=not_installed_not_loaded

Original KEXT offline verifier:

    IOKitPersonalities: Dict { }
    Code Signing Failure: not code signed
    ...FCIntensityPCIDiagnostic.kext appears to be loadable (including linkage for on-disk libraries).
    OFFLINE_VALIDATION_EXIT=0

The SDK linker warned that included libkmod objects were built for macOS 10.13, newer than the 10.12 target. The KEXT **was NOT signed, installed or loaded** and is NOT claimed to be runtime-compatible. Kernel startup is untested.

SHA-256 of the resulting x86_64 offline KEXT executable:
d485ff45e4be0f4576eb9c190035e0383bf9fc5f0794b370f55f6793ff834395.

The compiled code contains linked symbols fc_a124_safety_init, fc_a124_note_inventory, fc_a124_request_playback. Actual kextstat showed the original Blackmagic 10.4 driver still loaded and the independently owned diagnostic NOT loaded. The original device reported busy=0.

## Gate decisions, honest completion

- G0: **PASS, software stop controls**. Physical Panasonic electrical STOP remains **OPEN**.
- G1: **PARTIAL, static A124 output dispatch verified**. Real transmitter OFF register and effects **UNKNOWN**.
- G2: **PASS, source-owned synthetic-only safety core** (48/48 tests on each of 3 hosts).
- G3: **PASS, offline Sierra KEXT compiled/link-checked**. It is unsigned, nonmatching, uninstalled and untested as a live kernel driver.
- G4: **NOT ACCEPTED**. An actual recoverable test boot and exclusive physical PCI ownership has not been demonstrated.
- G5–G10: **NOT ACCEPTED**. No independent MMIO access, electrical HDMI OFF, stable Panasonic output, hardware input capture, or final replacement driver.

## Next actual engineering path

Continue G1 by tracing the root/base/A124 output configuration and sink-capability method call chains to the real register-level TX OFF and PHY handling, including readback and failure semantics. Extend the owned control core only from verified protocol evidence. Do not execute guessed writes or compete with the original 10.4 KEXT on the current headless boot.

A separate recovered test boot or an independently attached and isolated hardware host is required before **active** driver takeover, but not before this ongoing static/software work. Keep the owner's existing OUTPUT_DISABLED stop marker and two disabled launchers intact. No physical cables were touched.

Genuine physical success still means the Panasonic stops cycling with the cable attached, eventually displays changing generated images with stable lock, and produces actual captured HDMI pixels under an independently owned driver.
