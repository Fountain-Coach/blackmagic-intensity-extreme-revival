# Real Silver Mac mini analog stereo recording acceptance — 2026-10-25

## Fixed wiring / scope

- User reported and photographed the silver Intel Mac mini, Samsung display, Blackmagic Intensity Extreme via Thunderbolt only, Phonic MU802 and analog plugs.
- Sierra reports a headphone jack detected (`jack_connected=1`, `source=hdpn`), 2-channel analog Built-in Output `AppleHDAEngineOutput:1B,0,1,2:0`, now default.
- Sierra independently detects its analog Line In (`jack_connected=1`, source FourCC `line`, mute 0), 2-channel Built-in Input `AppleHDAEngineInput:1B,0,1,1:2`.
- Two analog jacks detected **does not** prove their cables are fed to/from the correct Phonic mixer connectors. It does prove the Mac's physical inputs are enumerated and selected.
- The Blackmagic PCIe A124 remains detected over Thunderbolt. Both Blackmagic HDMI connectors remain unused; `OUTPUT_DISABLED` remains intact. Blackmagic media playback and capture are not part of this recording test.

## Native program and acceptance

Source: `tools/sierra/silver-linein-capture.c`.

- AudioQueueNewInput native macOS Sierra Core Audio capture; exact built-in Line Input selected, not the arbitrary default; safety preflight reads jack, `line` source, mute and original machine model; no software monitoring, echo, device routing changes, system audio defaults or driver modifications.
- Captures stereo 16-bit PCM at nominal 44,100 samples/sec to a private `0600` WAV; verifies completion, headers, sample count and success code.
- 4s baseline `/tmp/fc-silver-analog-line-in-baseline-20261025.wav`: 176,400 frames, 705,644 bytes, no capture errors. First second electrical settling transient, then input near noise floor.
- 12s simultaneous test `/tmp/fc-silver-phonic-analog-roundtrip-20261025.wav`: 529,200 frames, 2,116,844 bytes, capture exit 0. Eight-second score started ~2s after capture began and accepted on the Mac's exact headphone output: 352,800 stereo frames, 8/8 completed buffers, native device running observed, playback exit 0. Captured input during the test stayed near its noise floor (about 4 to 5 PCM counts AC RMS, approximately -78 dBFS), so **the return path did not carry the test phrase**. Distinguish simultaneous audio processing from a true audible loopback.
- New `POST /api/record` in source-owned `tools/sierra/intensity-thunderbolt-score.py`, explicitly user-initiated by `Record Mac Line In (8 s WAV)` in `tools/sierra/intensity-thunderbolt-score.html`. Saves privately to `~/Music/Fountain Coach/Die Maschine/Recordings/` and analyzes DC-removed RMS after the first second to avoid mistaking settling spikes for recorded material. Does not automatically start or upload/share recordings.
- Live Safari service e2e acceptance: `POST /api/record` returned `LINE_INPUT_RECORDED`. Actual file `/Users/bene/Music/Fountain Coach/Die Maschine/Recordings/fc-die-maschine-line-in-20261025-105328.wav`, 1,411,244 bytes, mode `0600`, 44.1 kHz 16-bit stereo, duration 8.0 seconds. Recorded signal L=-79.6 dBFS / R=-79.8 dBFS after first second, below the -55 dBFS energy threshold. Again essentially silent, not a verified Phonic sound.
- Local server `GET /api/recording/latest.wav` and `HEAD` revalidated after service restart: HTTP 200, `audio/wav`, expected length, valid WAV downloaded locally. Server binds **127.0.0.1**, only on Silver Mac. External iPad cannot open this localhost address; use Mac Safari, private local Mac folder or a user-approved copy.
- Native source and runner retained in Git repo `Fountain-Coach/blackmagic-intensity-extreme-revival`; `tools/sierra/run-intensity-thunderbolt-score.sh` builds the capture program on the original Sierra Mac. No GitHub CI.

## Physical follow-up and feedback caution

Phonic MU802 official manual confirms that 2T RTN is a playback return, with its own level and buttons `TO MAIN L/R` and `TO CTRL RM/PHONES`. REC OUT provides an output of the main mix. For an audible recorder signal, inspect whether the Phonic Main mix actually has signal, its physical routing/level, that a REC OUT cable really reaches the Silver Line In, and the monitoring system. Without that, software cannot force the input waveform to become non-silent. Be mindful of **feedback** when sending a playback return to main while simultaneously recording: keep software input monitoring off, do not create a regenerative path from the Mac's Line In back to its headphone output, and begin with low levels. The official manual differentiates 2T Return playback from Record Out and explains the potential feedback loop.

## Accurate claim boundary

**Working:** Sierra physically recognizes both analog sockets; native, repeatable, actually sampled WAV capture; user-controlled private in-browser recording and retrieval; playback to Mac analog headphone output; Blackmagic Thunderbolt-only link left unchanged.

**Not yet working / proven:** non-silent Phonic-to-Mac recordings, intelligible audio, captured Blackmagic HDMI audio/video, software-mediated internal Intensity frame-buffer readback, independent audio hardware inspection beyond driver callbacks.

Always retain the observed silence in reports. Do not label the 12s simultaneous test a successful hardware loopback.
