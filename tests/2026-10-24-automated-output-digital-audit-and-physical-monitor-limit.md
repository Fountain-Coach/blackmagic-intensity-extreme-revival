# 24 October 2026 — autonomous HDMI recording and digital comparison; physical sink proof blocked

## User request and acceptance rule

**Request:** "record it and compare! is this possible, you don't need me to test this e2e."

**Conclusion:** The digital **original physical Blackmagic HDMI IN recording → decoded frame → Blackmagic HDMI OUT API submission** has been checked automatically, with a separately run source/hash/log auditor. The **external monitor's displayed pixels have not been and cannot be recorded** through the currently available hardware paths. A display is a sink, not an independent capture interface. To claim **physical HDMI OUT optical/electrical e2e**, the outgoing signal needs a separate instrumented capture input, or a confirmed camera/image sensor viewing the monitor. We do not have a second HDMI ingest device (silver: no independent camera; separate Ubuntu Mac mini: no `/dev/video*`). The other authorized Mac has a FaceTime camera, but its physical orientation relative to the new monitor is unknown: **do not secretly activate it or treat it as an HDMI receiver**. No user observation is required for the digital gate.

## Independent digital audit — actual Sierra host

Source file `tools/sierra/audit_blackmagic_hdmi_handoff.py` ran on actual silver Sierra 10.12.6, independently reading two previously captured actual physical source JPEG files with verified private ownership, full SHA-256 and correct JPEG markers and sizes. It independently parsed the earlier genuine vendor output submission log; it did NOT fabricate pixels, open the vendor card, claim HDMI wire inspection, or copy images to public Git.

Actual observed auditor output:

```text
stage=independent_digital_audit result=PASS
  source_real_captures=2
  hardware_output_accepts=4
  source_hash_refs_matched=4
  handoff_buffers_byte_identical=4
  monitor_pixels_recorded=0
  full_HDMI_wire_E2E=NOT_ACCEPTED
  report=/Volumes/Fresh/FountainVideoDockerLab/private-roundtrip/digital-handoff-audit.json
```

Private 0600/0700 audit metadata on silver:

- `/Volumes/Fresh/FountainVideoDockerLab/private-roundtrip/digital-handoff-audit.json`
- Two original captured physical JPEGs (user-private `/tmp/fc-intensity-extreme-physical-frame{0,1}.jpg`): 30,578 and 30,532 bytes, SHA-256 `ea1f51836311a496d91f493f82bb9e380f41efff76992ef8438cea6511c562d3` and `ab1abfddb80eb4a7d1404272aefed88d55373fd344dbb1debdc63a19c0c32634`.
- Four logged `IDeckLinkOutput::DisplayVideoFrameSync` returns zero; source JPEG hashes match exact expected order [0,1,0,1] and corresponding driver-allocated mutable YUV output buffers compared equal before submission.

**Scope:** This verifies **what the software submitted to the vendor interface**, NOT what voltages and pixels traveled through HDMI OUT. Those cannot be measured by reading back a caller-owned buffer. The two source JPEGs were captured earlier on the original HDMI input, not freshly while the extended output monitor was attached. The 17 physical frames received by same-silver Linux Docker previously remain independently witnessed, but they are also upstream of the new HDMI OUT monitor.

## Attempt to restore freshly recordable physical source — tested, unsuccessful

1. Sierra `pmset` reports `displaysleep=0`, system not entering sleep. The BMD display is still `main, online, active` at **1920×1080 30.000Hz**, source/vendor ID 8961. The vendor original 10.4 driver remains loaded.
2. Source-owned `src/silver-hdmi-same-mode-refresh.cpp`: read-only identity check passed, explicitly reasserting **exactly the same** current 1080p30 display mode returned `CGDisplaySetDisplayMode rc=0`, after verification unchanged. Native Fountain Video capture still failed with `no HDMI signal lock`, exit 68.
3. Sierra HDMI source framebuffer witness observed identical framebuffer hashes one second apart while idle. Source-owned `src/silver-fixed-hdmi-moving-test-pattern.mm` then displayed a non-private changing numbered frame pattern over **only that existing silver HDMI display**, leaving resolution and source unchanged. The framebuffer hashes changed between consecutive samples:
   - before `abf830bc7e7d941b27288d21fb596e0878e88f85ad660da2f27889b96a077f39`
   - after `3a8a3ebca59a9279cd98eeb183c534952b22e9f72e20dda04e2d9b5fc58a0464`
4. **Despite active changing source pixel data**, independent installed native AppKit actual-BMD capture again reported `No HDMI signal lock. Capture NOT started`, exit 68. No frame from that new pattern was accepted. This is NOT a stale compositor alone; actual *electrical HDMI source wire signal* remains unmeasured.
5. First 20s pattern rendered 200 frames and removed the test window but encountered a Sierra AppKit exit SIGSEGV during final teardown. Source-only diagnostic was repaired to order out window and terminate after flush, and separately recompiled/retested with a 3-second 30-frame pattern: **exit 0** and `window_removed=1`. No driver changes.
6. `src/decklink-hdmi-output-sink-readonly.cpp` attempted to query output-sink EDID from installed legacy `IDeckLinkStatus::GetBytes('edid')` without activating output. It returned `rc=0x80000003, bytes=4096`, **no usable EDID available** on this driver. A `GetInt('cvom')` returned a mode code but is **NOT** proof of a physical connected sink. No EDID, optical pixels or sink waveform were recorded.

**No unsupported conclusion about why the formerly working original HDMI IN does not currently lock.** The physical wiring, signal electrical state, legacy vendor internals, monitor handoff and original HDMI clock path remain possible independent reasons. The user-fixed HDMI source must not be replaced to make the test pass artificially.

## What is and is not automatically proven

| Measurement | Result |
| --- | --- |
| 1080p30 BMD HDMI source display present and active | PASS |
| Source display framebuffer advances with deterministic 20s pattern | PASS |
| Identical current display timing reasserted safely | PASS, did not restore capture |
| Previous real Blackmagic HDMI input frames privately recorded | PASS (earlier run) |
| Input lock and *fresh* full 30-frame capture now | **FAIL: locked=false**, zero accepted frames |
| Output mode 1080p30 YUV accepted by original vendor | PASS |
| Generated 6s output bars submitted by original Blackmagic | PASS at vendor API boundary |
| Two true historical capture JPEGs decoded, output twice each | PASS at vendor API boundary |
| Independent on-disk source/hash/log comparison | PASS, 4/4 |
| Actual HDMI OUT electrical payload captured by a separate receiver | **NO RETURN CHANNEL** |
| Actual connected monitor pixels recorded | **NOT TESTED / NO RETURN CHANNEL** |
| Full fresh source→capture→replay→independent monitor *physical* e2e | **NOT ACCEPTED** |

**Next physical instrumentation required:** a truly independent second HDMI input that can record the original Intensity Extreme's output *without removing the Mac → Intensity input source*, or a confirmed optical sensor pointed at the output monitor with calibrated pattern comparison. Once a return signal exists, automate brightness/geometry, frame IDs and time alignment with the source-owned comparators. Do not ask the human to judge colors manually and do not consider a software echo as a separate monitor witness.

PR #10 remains DRAFT. Original Sierra, Blackmagic 10.4 KEXT, fixed silver HDMI cable arrangement, Linux guest and installed Fountain Video application unchanged. All newly created source is checked into the branch; private pixels remain on the silver Mac only.
