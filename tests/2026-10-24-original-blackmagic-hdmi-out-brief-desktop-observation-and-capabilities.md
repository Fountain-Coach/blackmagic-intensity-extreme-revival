# 24 October 2026 — eyewitness: Sierra desktop appeared briefly on HDMI OUT at first monitor insertion

## User's original hardware observation

> The **first time** an external monitor was connected to the ORIGINAL Blackmagic Intensity Extreme's HDMI OUT, the monitor displayed the silver Mac Sierra desktop for approximately **one to two seconds**. Then that image disappeared, and it **never reappeared** in the subsequent test attempts.

Treat this as a **user-reported optical observation**, not a generated image, simulated frame, automatically recorded monitor signal or proof of continuous passthrough. The precise timing of application startup, original driver initialization and HDMI connection renegotiation relative to the 1–2 seconds was not recorded. Do not overwrite or reinterpret this observation as "monitor never showed anything."

The rig remains EXACTLY the user's authorized fixture:

**Silver Macmini5,3 HDMI OUT → original Intensity Extreme HDMI IN → Thunderbolt back to SAME silver Sierra 10.12.6; ORIGINAL Intensity Extreme HDMI OUT → newly attached reference monitor.**

## Read-only device capability measurements, performed after that observation

Source-owned `src/decklink-output-idle-config-readonly.cpp` now reads both legacy configuration and **IDeckLinkAttributes** without changing the hardware, driver, PCI state, firmware, OS, HDMI source, monitor or user-private pixels.

Observed on actual original 10.4 driver, while the installed Fountain Video application remained running:

```text
stage=original_idle_config key=idle_output_operation rc=0x00000000 value_fourcc_or_flags=0x626c6163  # blac = black when idle
stage=original_idle_config key=default_output_mode rc=0x00000000 value_fourcc_or_flags=0x48703330 # Hp30, corrected while GUI runs
stage=original_idle_config key=capture_passthrough rc=0x80000008 value_fourcc_or_flags=0x00000000
stage=original_passthrough_flags key=black_during_capture_bvoc rc=0x80000001 value=UNSUPPORTED
stage=original_passthrough_flags key=hardware_bypass_byps rc=0x80000003 value=UNSUPPORTED
stage=original_output_capabilities attributes_rc=0x00000000 present=1
stage=original_output_capabilities flag=supports_full_duplex_fdup rc=0x00000000 value=FALSE
stage=original_output_capabilities flag=has_hardware_bypass_byps rc=0x00000000 value=FALSE
stage=original_output_capabilities flag=supports_idle_output_idou rc=0x00000000 value=TRUE
stage=original_idle_config modified=0 persisted=0 rebooted=0
```

## Engineering interpretation

1. **The brief desktop image is genuine eyewitness evidence that image content appeared on the new external monitor at first HDMI OUT connection.** It is not a sustained signal-lock, capture, or pass-through acceptance.
2. **The actual Intensity Extreme under driver 10.4 reports neither full-duplex operation nor hardware bypass.** The modern `CapturePassThroughMode` configuration is not available as a successful read, nor are the two legacy flags. It is NOT justified to assume this card can be used as a permanent transparent HDMI splitter or input-monitoring-through-output device. Do not infer that this device implements modern SDK 'pdir' / 'pcln' passthrough just because modern Blackmagic documentation mentions them.
3. The corrected native Fountain Video GUI holds **Hp30** (1080p30) for the card's output default, **but retains `voio=blac`**. Its presence therefore does not mean it is continuously providing Sierra desktop pictures to HDMI OUT. It does not itself call `EnableVideoOutput` or `DisplayVideoFrameSync` during idle monitoring.
4. Plausible hypotheses for the transient are a start-up/hotplug routing interval, a buffered prior frame, or short-lived device output before control/idle state is applied. **None is established.** A previously seen desktop for 1–2 seconds does not independently prove which internal route carried it or why it disappeared.
5. **Keep monitor on/off cycling separate from disappearance of picture content.** Black when idle is a deliberate pixel policy, not proof of an interrupted HDMI electrical clock. The 40-second output test previously delivered 918 YUV frames without vendor API errors, matching 1080p30 throughout, but optical monitor continuity was never independently recorded.
6. The best supported function of this unit is the verified **sequential** workflow: actual HDMI IN capture → private recorded frames → stop input → HDMI OUT playback. Same-silver Docker can process the recorded or live-acquired stream, but the unit does not advertise simultaneous full-duplex input/output.
7. A truly independent monitor pixel/electrical capture requires a return sensor. No additional camera, second HDMI capture box or alternative original signal source should be silently invented or introduced.

**Status:** Keep the operator's 1–2 second optical sighting as a real event. **Sustained passthrough unsupported by reported capabilities**; full newly recorded input-to-output roundtrip remains open while physical HDMI IN signal lock fails. No physical or OS changes made in this observation/probe.
