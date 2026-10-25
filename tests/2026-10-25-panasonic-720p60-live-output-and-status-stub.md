# Panasonic HDMI output: live 90-second video score, original status limitation and guarded installer

**Date:** 25 October 2026, original silver Macmini5,3, macOS Sierra 10.12.6, Blackmagic Desktop Video 10.4, original Thunderbolt Intensity Extreme A124.

**Original physical wiring unchanged:** silver Mac HDMI OUT -> Intensity HDMI IN; Intensity HDMI OUT -> Panasonic HDMI IN. The user previously observed the Panasonic switching on/off, but **no picture was independently observed on the Panasonic during these tests**.

## Outcome

Our source-owned *Die Maschine* frame generator sent **5,400/5,400** 720p60 UYVY frames over **90.001 seconds** through the real original Blackmagic device. The hardware reported **device_busy_state=0x2 (playback)** from a second independently launched process during playback, as opposed to **0x0 (idle)** beforehand. The driver accepted every frame, returned success on DisableVideoOutput and was observed **idle again** after completion.

This proves a live **hardware playback session and accepted frame submissions**, not electrical TMDS signal lock, actual HDMI sink EDID, successful monitor reception or visual pixels on the Panasonic.

## Exact read-only output status, configuration and device activity

Original Desktop Video 10.4 exposes the `IDeckLinkStatus` interface (IID `5F558200-4028-49BC-BEAC-DB3FA4A96E46`). Its `GetInt` implementation reported:

```text
stage=video_output_status status_interface_rc=0x00000000 available=1
stage=video_output_status key=current_output_mode rc=0x00000000 value=0x68703630
stage=video_output_status key=current_output_flags rc=0x00000000 value=0x00000000
```

`0x68703630` is the original supported **720p60** mode.

Some status IDs were not supported: `last_output_pixel_format` and `current_input_mode` returned `0x80000008`; the status-interface `busy` returned `0x80000003`. Those responses do **not** prove the signal is missing. The separate **IDeckLinkAttributes** busy-state property is implemented, and returned actual busy state while playing:

```text
before: stage=attribute name=device_busy_state status=0x00000000 value=0x0
during: stage=attribute name=device_busy_state status=0x00000000 value=0x2
after:  stage=attribute name=device_busy_state status=0x00000000 value=0x0
```

Read-only `IDeckLinkConfiguration::GetInt(video_output_connection)` returned mask **0xA**, which includes the HDMI connection bit **0x2**. Other available output connections may coexist; the configuration was **not** changed.

## Important original 10.4 EDID-status incident — a stub, not a buffer-size request

The initial status probe incorrectly assumed modern `IDeckLinkStatus::GetBytes` implements `bmdDeckLinkStatusReceivedEDID` on the original 10.4 runtime.

The first invocation supplied a non-null writable buffer and returned **`0x80000003`** while changing the supplied size to **4096**. We initially misread this as a recoverable small-buffer result. A second attempt supplied a null output buffer to request its size and triggered a **SIGSEGV in user space**. The original host did not reboot, Blackmagic 10.4 drivers stayed loaded, and independent I/O APIs confirmed the hardware idle afterward.

The actual Sierra crash report names:

```text
Exception Type: EXC_BAD_ACCESS (SIGSEGV)
KERN_INVALID_ADDRESS at 0x0
Thread 0 Crashed:
CDeckLinkStatus::GetBytes(unsigned int, void**, unsigned int*) + 4
```

We independently disassembled the **exact installed** `/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI` binary. The CDeckLinkStatus::GetBytes implementation at text offset **0x1f3e4** is a constant unsupported-operation stub:

```text
0x1f3e4: pushq %rbp
0x1f3e5: movq %rsp,%rbp
0x1f3e8: movq $0x0,(%rdx)
0x1f3ef: movl $0x1000,(%rcx)
0x1f3f5: movl $0x80000003,%eax
0x1f3fa: popq %rbp
0x1f3fb: retq
```

That explains **both** why the first call reported a fixed 4096 bytes and why a null third argument crashed. It does **not** retrieve an HDMI EDID. The method has no device-state read and no EDID data acquisition. No other buffer-size retry is warranted.

[The corrected source-owned read-only status probe](../src/decklink-hdmi-sink-status.cpp) contains **no GetBytes call or typedef**. It queries only demonstrated integer statuses and reports the Panasonic EDID and actual on-wire pixels as unavailable or unmeasured. [Five crash-regression guards](test_legacy_status_no_getbytes.py) passed **5/5 on Sierra Python 2.7, modern macOS Python 3 and Ubuntu Python 3**.

**Do not re-enable original 10.4 GetBytes; do not install a new driver, remove SIP, edit PCI registers or replace system frameworks merely to obtain EDID.**

## Actual 90-second creative playback

First run with source-owned [video output adapter](../src/fountain-video-output.cpp) (original 10.4 DeckLink only as physical output transport, not Black Syphon):

```text
stage=trial_start_time 2026-10-25 06:01:03 +0100
stage=output_enable mode=720p60 size=1280x720 fps=60 rc=0x00000000
stage=frame_create rc=0x00000000 width=1280 height=720 bytes=1843200
stage=physical_score progress_sec=1 frame_submissions=60 generated_poses=10 frame_crc=0x2955b1cd
stage=physical_score progress_sec=90 frame_submissions=5400 generated_poses=900 frame_crc=0xd30a5081
stage=physical_score result=DONE mode=720p60 submitted=5400 attempted=5400 generated_poses=900 elapsed_s=90.001 hdmi_wire_image=UNVERIFIED
stage=disable_output rc=0x00000000
stage=complete exit=0 capture_enabled=0 input_hdmi_replugged=0 playback_only=1
stage=panasonic_trial trial_exit=0
stage=trial_end_time 2026-10-25 06:02:33 +0100
```

Source-owned video content changed (distinct UYVY frame fingerprints). This is stronger than a one-frame test and verifies bounded sustained playback. Neither the Panasonic image nor physical sink timing is independently witnessed.

## Preemptive source-owned hardware-ownership guard

We hardened the native adapter to block hardware output unless ALL of the following are true:

1. Original **Macmini5,3 on Sierra (Darwin 16.x)**, not another Mac or host.
2. Detected physical model exactly **Intensity Extreme**.
3. `IDeckLinkAttributes::GetInt(device_busy_state)` succeeds and equals **0** before use.
4. `IDeckLinkConfiguration::GetInt(video_output_connection)` succeeds and includes **HDMI bit 0x2**.
5. Output mode is explicit original-model supported 720p60 or 1080p25; bounded duration **5–120 seconds**.
6. Capture never enabled; output disabled and frame/objects released at the end, with SIGINT/SIGTERM handlers.

During the active 90-second playback, **a second invocation of the newly guarded binary** observed `busy=0x2`, printed `DEVICE_BUSY_OR_UNKNOWN` and returned **23**. It did not attempt to take over or enable another output stream.

[Seven static guard-contract tests](test_fountain_output_guard_contract.py) passed **7/7 on Sierra, modern macOS and Ubuntu**.

We ran the [source-owned Sierra installer](../tools/sierra/install-fountain-video-instrument.sh) with the updated adapter, retaining the prior bundle under `~/Library/Application Support/Fountain Coach/Video Instrument/tmp/last-good-app.20261025060316` for rollback.

The installed guarded adapter then independently passed a real **five-second 720p60 test**:

```text
stage=hardware_guard busy_read_rc=0x00000000 busy=0x0
stage=hardware_guard output_config_rc=0x00000000 selected_output_mask=0xa HDMI_bit=0x2
stage=hardware_guard result=PASS original_silver_sierra=1 Intensity_Extreme=1 device_idle=1 HDMI_output_selected=1
stage=output_enable mode=720p60 size=1280x720 fps=60 rc=0x00000000
stage=physical_score result=DONE mode=720p60 submitted=300 attempted=300 generated_poses=50 elapsed_s=5.006 hdmi_wire_image=UNVERIFIED
stage=disable_output rc=0x00000000
stage=complete exit=0 capture_enabled=0 input_hdmi_replugged=0 playback_only=1
after: stage=attribute name=device_busy_state status=0x00000000 value=0x0
```

The creative renderer, native Syphon interprocess pixel proof and GUI preview are [covered separately](2026-10-25-fountain-creative-instrument-physical-output.md). The physical hardware-output code still uses the original 10.4 vendor runtime, and replacement-driver research remains independent from this working creative path.

## What is still necessary to claim physical Panasonic success

We cannot derive Panasonic HDMI sink EDID or confirm a picture from original 10.4 `IDeckLinkStatus`; its EDID implementation is a hardcoded unsupported stub. We also have no independent video camera, HDMI waveform/InfoFrame analyzer, or sink-status interface exposed by this exact A124 connection.

An independent **Panasonic sink-capabilities calibration** would require a physical test—for example, temporarily connecting the silver Mac HDMI OUT directly to Panasonic HDMI IN so Sierra can inspect that sink's real EDID, then restoring the Mac → Intensity input and Intensity output → Panasonic topology. Such replugging must be explicitly coordinated with the owner, who has authorised physical plug moves but no manual software configuration. It has **not** happened yet.

Until a visual or receiver-side witness is supplied, **Panasonic HDMI lock and visible image: OPEN**. Do not promote 5,400 accepted API frames to optical HDMI success.

**D4 capture is also OPEN** because Blackmagic HDMI input receiver lock never passed earlier tests.
