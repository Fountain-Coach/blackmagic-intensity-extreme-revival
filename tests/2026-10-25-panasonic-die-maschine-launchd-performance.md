# Die Maschine on Panasonic: four-movement creative performance and Sierra launchd ownership

**Date:** 25 October 2026, Europe/Berlin.  
**Hardware:** original silver Macmini5,3, macOS Sierra 10.12.6, original Blackmagic Intensity Extreme (PCI BDBD:A124), original Desktop Video 10.4.  
**Physical topology unchanged:** silver Mac HDMI OUT → Blackmagic HDMI IN; silver Mac Thunderbolt → Blackmagic; Blackmagic HDMI OUT → Panasonic HDMI IN.

## User intent

Display **actual Fountain-generated creative imagery on the Panasonic**, not a generic chat image, an empty UI panel, or a software-only simulation. User handles only physical plug changes; all software and maintenance is agent-owned.

## Creative score: four source-owned movements

[Single shared integer-only image kernel](../src/fountain-video-score.h) now exposes four distinct cinematic movements, each switching after 600 output-frame counts (every 10 seconds at 720p60):

1. **Eye / witness field:** dark geometry, orbiting articulated rings, diagonal scanning, gold/red witnesses
2. **Amber shot/reverse:** copper and cyan divisions, high-contrast striping and central cut
3. **Silver negative:** inverse field, mobile window, red circle and vertical cut
4. **Die Maschine:** near-black composition, crossing signal traces and source-owned pixel-font title

Frames use the **same creative source** for independently running Syphon RGBA/OpenGL and Blackmagic 8-bit UYVY transport; this is not a claim that their running clocks are synchronised.

[Pure frame test](test_fountain_video_score.c) verified **determinism, distinct images across all four movements and distinct RGBA/UYVY frame fingerprints** on original Sierra, modern macOS and Ubuntu. The same stage fingerprints were witnessed on all three:

```text
stage=video_score_movements result=PASS modes=4
rgba=ac0ab377,b1cd0771,1b0cbde6,8ecc9a16
uyvy=3db5d05c,f4c81d28,1422969b,e2f1f872
```

A source-owned Sierra installation successfully compiled and installed the updated Fountain Video Instrument app and physical output adapter. Invalid playback durations (0, 21 and nonsensical values) were rejected before hardware access with exit **64**.

## Physical playback — original output transport

An initial bounded creative performance was run through the original Blackmagic DeckLink output API: **7,200/7,200 frame submissions at 720p60 across 120.004 seconds**, zero output API errors, and a clean DisableVideoOutput. This supplied actual output frames to the physical hardware, but no independent Panasonic optical witness was available.

The expanded four-movement composition was then started as a twenty-minute process via a detached SSH/nohup job. That job **stopped after 46.485 seconds** (2,785 successful frame submissions) instead of the requested 20 minutes. Its then-current code printed `result=DONE` despite the early interrupt: **this was an inaccurate report, not a successful 20-minute performance**. A subsequent SSH-owned run was also observed to end early. The exact origin of the termination signal was not established.

[The hardware output adapter](../src/fountain-video-output.cpp) was corrected to store SIGINT/SIGTERM and report `result=INTERRUPTED`, submitted frame counts and nonzero exit code on early termination.

## Production ownership correction: Sierra launchd, not an SSH shell

Created and installed a **user-owned GUI launchd service** with label `coach.fountain.video.panasonic` and verified it registered in `gui/502`.

- [Source-owned launchd installer](../tools/sierra/install-fountain-panasonic-launchd.sh)
- [Fountain-owned start/status/stop controller](../tools/sierra/fountain-panasonic-show.sh)
- Service process: installed `~/Library/Application Support/Fountain Coach/Video Instrument/bin/fountain-video-output`
- Arguments: `--mode 720p60 --minutes 20`
- Explicit **RunAtLoad=false** and **KeepAlive=false**. The show is not set to start at login, restart automatically or run forever.
- Bounded maximum: **20 minutes**, with SIGTERM/SIGINT handling and cleanup.
- Service stdout/stderr captured in the Fountain Video Instrument run directory.
- User need not operate a GUI, terminal, security setting or software.

After observing the hardware idle, the previous playback binary was backed up, the corrected binary installed, and the launchd job started via `launchctl kickstart -p`. **Sierra reported launchd state `running` with pid 8017**, and the physical Intensity Extreme reported **busy=0x2** independently from its DeckLinkAttributes interface.

Independent read-only heartbeats witnessed ongoing progress at **31 seconds, 51 seconds and 91 seconds**, beyond the prior SSH-owned early-stop boundary:

```text
stage=show result=RUNNING lease=launchd max_minutes=20
state = running
pid = 8017
stage=physical_score progress_sec=90 frame_submissions=5400 generated_poses=900
stage=physical_score progress_sec=91 frame_submissions=5460 generated_poses=910
stage=attribute name=device_busy_state status=0x00000000 value=0x2
```

**Caveat:** these are live heartbeats, not a witnessed *completed* 20-minute run. The service auto-stops after its allotted duration; it does not run as a repeating automation.

## Installation safety

[The owned Sierra application installer](../tools/sierra/install-fountain-video-instrument.sh) was amended to refuse an installation if this service is actively playing. The interlock was physically verified **during the active show**:

```text
install=BLOCKED reason=active_panasonic_show
stage=installer_interlock result_code=30 expected=30
stage=attribute name=device_busy_state status=0x00000000 value=0x2
```

It also retains a last-good copy of the hardware output binary and installs the owned launchd controller for subsequent maintenance.

No Blackmagic kernel extension, SIP setting, firmware, capture stream or external HDMI cable arrangement was changed. The old original 10.4 driver remains the **temporary hardware-output adapter dependency**; our visual generation and Syphon publication are independent of the proprietary Black Syphon application.

## Precise acceptance boundary

| Boundary | Result |
| --- | --- |
| Original source-owned four-movement composition | **PASS**, pixel tests on three machines |
| Source-owned Sierra app and HDMI adapter installed | **PASS** |
| Bounded 120-second hardware frame submission | **PASS**, 7,200/7,200 |
| Properly handled interrupted background playback | **SOURCE FIXED**, signal-aware reporting |
| launchd owns active Panther/Panasonic show rather than SSH | **PASS**, actual user GUI domain |
| Independent physical Blackmagic busy-state during show | **PASS**, busy=0x2 |
| Installer refuses replacing active video engine | **PASS**, exit 30 |
| HDMI receiver lock as measured by Panasonic | **NOT WITNESSED** |
| Visible *Die Maschine* image on Panasonic screen | **NOT WITNESSED** |
| Full 20-minute performance completed | **NOT YET VERIFIED AT REPORT TIME** |
| Original Blackmagic driver replaced | **NO**, intentionally unchanged |

The output engine demonstrably drives the actual Blackmagic playback path; **electrical HDMI link and Panasonic optical display still require independent witness**. Do not label vendor-success frame submissions as visually confirmed image output.

**Hardware D4 acceptance remains OPEN** (input capture lock and Panasonic physical image separately).
