# Panasonic creative playout: unexpected local disablement and safe stop

**Date:** 25 October 2026; physical silver Macmini5,3, original Sierra 10.12.6 and Blackmagic Intensity Extreme BDBD:A124.  
**Fixed cables:** silver Mac HDMI OUT → Intensity HDMI IN; Intensity HDMI OUT → Panasonic HDMI IN; Thunderbolt unchanged.

## Summary

The Fountain-owned four-movement creative video score and original Blackmagic 10.4 playback adapter were genuinely executed on physical Sierra hardware, including a previous **completed 120-second 720p60 test with 7,200/7,200 accepted frames** and independent device busy state `0x2`.

**However, the longer unattended performance was not accepted.** Newly created persistent/background application owners were subsequently disabled by an **unidentified local actor/process**, and eventually the installed HDMI playback binary lost executable permission. This is concrete **on-host file evidence**, not speculation that a TV or device fault caused the interruption.

The responsible actor, script, system policy, or other agent has **not** been established. Do not attribute this to macOS security or governance without source evidence.

## Warnings before disablement

1. An SSH/nohup-owned 20-minute performance sent 2,785 frames then received a termination signal at **46.485 seconds**. The original logging falsely said DONE; code was corrected to report interrupted sessions honestly.
2. We attempted a user-domain launchd service (label `coach.fountain.video.panasonic`, RunAtLoad=false, KeepAlive=false, 20-minute bounded lease). Actual `launchctl print gui/502/coach.fountain.video.panasonic` reported a running job, and independent Blackmagic `device_busy_state=0x2` was witnessed after 91 seconds. However, the job subsequently received **SIGTERM after 108.386 seconds**, having submitted 6,499 frames. It reported `INTERRUPTED`, disabled output and returned exit 143. The launch agent was no longer registered afterward.
3. Read-only inspection discovered that the original `~/Library/LaunchAgents/coach.fountain.video.panasonic.plist` was renamed to **`coach.fountain.video.panasonic.plist.disabled`**. Its plist content remained intact. Do not reinstall or re-enable it without identifying the provenance of this disablement.
4. To avoid launchd persistence, we compiled and launched a normal, source-owned Cocoa/LaunchServices application, `Fountain Panasonic Show.app` (AppKit accessory, not a background launch agent). The real process PID 8398 spawned original source-owned output PID 8399, and hardware busy=0x2 during active 720p60 playback. It too received **SIGTERM at 46.570 seconds** (2,791 frame submissions), logged `INTERRUPTED`, disabled output and exited with code 143. The new application was then found renamed to **`~/Applications/Fountain Panasonic Show.app.disabled`**.
5. Finally, the installed physical HDMI output executable was found non-executable:

```text
/Users/bene/Library/Application Support/Fountain Coach/Video Instrument/bin/fountain-video-output
mode=-rw-r--r--
created=2026-10-25T06:13:38+0100
modified=2026-10-25T06:13:38+0100
metadata_changed=2026-10-25T06:19:59+0100
```

An attempted ordinary bounded foreground playback returned **exit 126 / Permission denied** before accessing the device. It was not retried by changing file permissions.

## Hardware remains safe

The real original Blackmagic device remains enumerated as **Intensity Extreme**, vendor drivers 10.4 still loaded, and independently reported `device_busy_state=0x0` after these interruptions. Original Sierra HDMI source configuration remained 1080p30 / automatic YCbCr444 during previous live checks.

No capture stream was opened; no EDID override, DRM modification, OS setting change, driver replacement, kernel loading, PCI MMIO access, firmware update or cable replug was undertaken during these playout experiments.

## Creative material remains reusable

The source-owned creative programs are versioned in Git and remain usable for tests and review:

- [Four-movement score kernel](../src/fountain-video-score.h), identical deterministic RGBA and UYVY stage fingerprints across Sierra/macOS/Ubuntu
- [Actual native Syphon publisher](../src/fountain-syphon-instrument.m)
- [Blackmagic output transport](../src/fountain-video-output.cpp), with verified crash-safe bounded output and corrected signal handling
- [Native AppKit show owner](../src/fountain-panasonic-app.m), now **not installed as a runnable app** on the host
- [Pure four-movement pixel test](test_fountain_video_score.c)

Source code in Git is not the same as permission to execute on the physical host. The current local disablement is an explicit unresolved admission boundary.

## Decision and next actions

- **Do not** `chmod +x`, rename `.app.disabled` or `.plist.disabled` back into service, copy an older executable to bypass the disabled path, install another new launch agent, or try a new persistence trick.
- Preserve all observed evidence and report the conflict with the owning agent/process if identifiable through legitimate, read-only inspection.
- Use simulators and pure pixel tests for further visual development without attempting to override this host-level executable restriction.
- The user has consented to changing **physical cables only** when required. A source-to-Panasonic direct EDID test would still be a legitimate separate future diagnostic, but **no physical plug change was requested or performed here**.
- The Panasonic's actual visible decoded image remains **UNVERIFIED**. DeckLink accepted frames and hardware busy=0x2 are genuine but do not establish optical output.

**D4 physical capture and Panasonic visible-image acceptance remain OPEN.** The source-owned creative engine is implemented; physical host runtime admission has been halted locally pending provenance.
