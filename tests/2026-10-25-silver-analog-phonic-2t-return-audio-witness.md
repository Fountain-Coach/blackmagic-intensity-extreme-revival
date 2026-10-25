# Silver Mac → Phonic 2T Return: analog audio witness

Date: 2026-10-25. User-established wiring: **Silver Mac mini analog stereo output → Phonic MU802 mini-jack stereo 2T Return**. The Blackmagic Intensity Extreme remains connected to Silver only via Thunderbolt; both of its HDMI ports remain disconnected. The Silver-to-Samsung display uses the Mac's HDMI separately. Do not reintroduce the earlier Lindy/HDMI audio extraction or Phonic microphone input as present assumptions.

## Observed on original Sierra host
- Machine `Uwes-Mac-mini` / Macmini5,3 / Sierra 10.12.6; the exact original Intensity Extreme PCIe `BDBD:A124` continues to enumerate over Thunderbolt.
- CoreAudio device `AppleHDAEngineOutput:1B,0,1,2:0`, **Built-in Output**: two output channels at 44,100 Hz. A separate HDMI default device remains selected (device 70); no system default was changed.
- CoreAudio native `tools/sierra/intensity-audio-score.c --phonic` explicitly selects Built-in Output, not the HDMI default. Its generative pitch is seeded from an actual read-only DeckLink output-mode status, currently `0x6e747363` (NTSC); this does not constitute a captured audio stream. Duration 8.0 seconds, low peak amplitude 0.020, 8 queued and 8 completed buffers, 352,800 frames at 44.1 kHz, native running state observed, zero stop error, exit 0.
- Same native 8-second test invoked through running Safari-local score API `POST /api/audio/phonic`: response `COREAUDIO_COMPLETED`, `device_accepted_frames=true`. The web front document was refreshed to `http://127.0.0.1:18743/?phonic=1` and screenshot witness showed **Hear via Phonic 2T Return (8 s, quiet)** and the independent **Send audio to Intensity Extreme** button.
- The retired HDMI/Lindy audio control was removed; `POST /api/audio/hdmi` now returns Not found. Standalone `--hdmi` is likewise not accepted in the source-owned audio program. The original `OUTPUT_DISABLED` Blackmagic video-playout guard remains intact.
- Owner's physical wiring report is authoritative for the cable placement. Software playback completion is **not** independent proof that the Phonic actually received or played sound; the owner's hearing or Phonic level meters must establish that final physical outcome.

## Phonic signal path
The Phonic mini-jack is its **2T Return** input, not the separate quarter-inch stereo AUX Return and not MIC 1. To monitor it, engage `TO MAIN L-R` or `TO CTRL RM/PHONES` on the Phonic as appropriate; start with low analog volume. No microphone preamp and no phantom power are required for this line-level connection. Do not assert the routing buttons or physical listening have been witnessed through software.

## Reproduction and ownership
The source-owned `tools/sierra/run-intensity-thunderbolt-score.sh` builds both the read-only DeckLink status probe and `tools/sierra/intensity-audio-score.c`, and launches the one-hour, manual, local-only Safari score. It changes no system audio default, does not start at boot, and does not enable the original Blackmagic video output. To replay the analog signal choose the Phonic button in Safari (or explicitly run the native binary with `--phonic` on the original Sierra Mac). Keep `OUTPUT_DISABLED` intact.

**Accepted now:** CoreAudio built-in analog device accepted the exact routed score; source and route are repeatable.  
**Unverified:** cable continuity, actual mixer meters, audible output, and any physical audio output from the disconnected Intensity Extreme.
