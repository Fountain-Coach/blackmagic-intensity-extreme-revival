# 24 October 2026 — silver Intensity Extreme HDMI monitor cycling: native idle-output repair

## Fixed, unmodified physical topology

Original silver Intel Macmini5,3, **macOS Sierra 10.12.6**, original Blackmagic Desktop Video KEXT **10.4**, original HDMI output of silver Mac → original Intensity Extreme HDMI IN; original Intensity Extreme HDMI OUT → external monitor; original Comdis dock/Thunderbolt 1 returns to **that same silver Mac**.

No driver/firmware/OS reinstall, cable change, substitute signal source, root permission escalation, device reflash, change to Linux Docker guest, or network broadcast.

## Reader-reported symptom

External monitor attached to original Blackmagic **HDMI OUT** continually powers/displays signal on/off/on/off. The source-owned video playback program previously enabled output for a few seconds at 1080p30 and then disabled output. Optical monitor state has no available readback sensor, and the driver does not expose independent HDMI OUT wire pixels.

## Diagnosed mismatch

Read-only original Desktop Video 10.4 configuration on physical silver:

- `dvom` / default output mode **Hi50 (1080i50)**.
- `voio` / idle output operation **blac (black)**.
- Fixed silver Mac main display source is **1080p30**.
- Blackmagic `cvom` status during previous idle read was **Hi50**, changing to **Hp30 (1080p30)** while video playback was enabled.

Thus every short playback enable/disable crosses 1080i50 ↔ 1080p30 and may prompt display sink resynchronization. This is a plausible cause of monitor cycling; **not an optical confirmation**.

## Actual sustained 1080p30 playback

Source-owned `src/decklink-hdmi-steady-output.cpp` was compiled on Sierra and ran a bounded single uninterrupted physical Blackmagic `EnableVideoOutput(Hp30)` session. It submitted generated YUV frames without turning HDMI output off between them, while independently querying `IDeckLinkStatus::GetInt('cvom')` every second.

Observed:

```text
stage=steady_output_heartbeat elapsed_s=40 vendor_frames_accepted=918 failures=0 output_enabled=1 mode_same=1
stage=steady_output_disable rc=0x00000000
stage=steady_output_complete result=API_STABLE vendor_frames=918 api_errors=0 consecutive_matching_mode_seconds=40 output_disabled_after_test=1 monitor_optical_pixels_unmeasured=1
```

This proves driver/API continuity; optical screen behavior itself was not recorded.

## Isolating idle standard versus idle image privacy

`src/decklink-output-idle-config-readonly.cpp` confirmed original idle black and Hi50. The first guarded 12-second test set Hp30 and `lafa`/LastFrame, holding stable mode. Because the signal source is the Mac's **private primary display**, LastFrame can retain desktop pixels unnecessarily and is **not** the selected fix.

The second, narrower guarded 8-second test changed only `dvom` to Hp30, retaining `voio` exactly `blac`. All eight readbacks reported Hp30 with idle black, status current mode Hp30, then original Hi50/black settings were restored and independently verified.

Blackmagic's persistent `WriteConfigurationToPreferences` returned **0x80000009 (E_ACCESSDENIED)** at the normal Sierra user privilege level. No sudo/admin escalation was attempted; the in-memory change was undone, the original setting was independently verified, and original settings were saved privately to:

`/Volumes/Fresh/FountainVideoDockerLab/private-roundtrip/original-blackmagic-output-settings-before-p30.txt`

## Implemented in installed native Fountain Video.app

`apps/fountain-video-sierra/FountainVideo.mm` now uses the installed proprietary `IDeckLinkConfiguration` user-space API:

1. Only on verified Macmini5,3 running the exact original 1920×1080@30 BMD HDMI display.
2. Confirm `voio=blac`, and permit only `dvom=Hi50` (legacy default) or `dvom=Hp30` (already corrected). Unknown values fail closed.
3. When original Hi50, temporarily set Hp30 while native Fountain Video owns the original Blackmagic device; retain configuration reference, verify `Hp30` and idle black. No `WriteConfigurationToPreferences`.
4. When original already Hp30, adopt existing mode without altering it.
5. **Explicit rollback** on normal app/device closure, only if *this application* changed Hi50→Hp30 and the card still reads Hp30/black. Before release, set original Hi50. If another client changed the mode meanwhile, do not overwrite its new state.
6. Leave idle frame black. No request to capture, play or publish main-desktop pixels is implied merely by this output-mode repair.

Source change implementing explicit restore: commit `0454654b2038a4bce36bfe6666426974a62860ea`.

### Hardware verification BEFORE installation

- Native app build: x86_64, Sierra; converter, virtual bars, preview/window, Scratch JPEG tests all passed.
- Actual DeckLink original configuration reset to **Hi50/black** using a guarded source-owned *volatile, nonpersistent* rollback helper. Independent preflight confirmed.
- New native candidate `--self-test-original-hdmi-output-idle` then set **Hp30/black** for 3 seconds; controller explicitly released its ownership and restored **Hi50/black**, return code 0.
- Independent **post-exit** Blackmagic configuration readback confirmed original **Hi50/black**, not merely that the candidate logged an attempted restore.
- No capture or output playback triggered in that native unit test.

### Installed on physical silver Mac and verified

The validated candidate replaced installed `~/Applications/Fountain Video.app` with an atomic same-volume move only after successful tests and binary SHA comparison. Previous installed version preserved:

`/Users/bene/Applications/.FountainVideo-before-output-standard-20261024.app`

New installed native x86_64 executable SHA-256:

`3804867ec6298b771df646f3b2fcf4be7ec799c0e669a61838f324af0b168f18`

The GUI was reopened and independently queried while running: original Blackmagic `dvom=Hp30`, `voio=blac`, Sierra still 10.12.6.

Four independent readback samples over **15 seconds** reported the same Hp30/black, and the actual `IDeckLinkStatus::GetInt('cvom')` output mode remained Hp30 on all four. The native GUI process remained alive throughout. Mailbox `GET /health` reported `last_host_commit=17` unchanged: **no additional private desktop frames automatically published**.

Finally, a **normal GUI quit/relaunch acceptance** passed:

- GUI running: output mode **Hp30**.
- After normal quit: mode **Hi50** restored, independently checked.
- After relaunch: mode **Hp30**, idle frame **black**, independently checked; GUI still running.

## Accepted and open gates

**ACCEPTED:** old default-mode mismatch proven; 40-second continuous Blackmagic 1080p30 API acceptance; exact in-memory idle-standard correction without LastFrame leakage; native app regression, rollback and re-launch tests; final app installed and holding Hp30; no admin requirement.

**OPEN:** Whether the external monitor's *optical* on/off cycling has stopped; output HDMI wire capture or optical sensor absent. **Do not equate vendor status or frame submission with independently witnessed monitor pixels.** Also, original HDMI IN physical signal lock is still reported absent in later input tests; no fresh 30-frame record→playback roundtrip accepted. Unattended Linux Docker reader longevity is separately open.

**User-fixed hardware interface and PR #10 draft remain authoritative.**
