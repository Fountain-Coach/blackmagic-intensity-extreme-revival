# 2026-10-25 11:06 CET — physical Phonic return recording regression test

## Exact test performed, without requiring visual meter observation

1. Read the original Sierra Mac mini's real Core Audio controls:
   - analog output jack connected = 1, source = `hdpn`;
   - analog input jack connected = 1, source = `line`, unmuted;
   - original Blackmagic `OUTPUT_DISABLED` marker remains present.
2. Started an exact-device native stereo WAV recording (Sierra Core Audio Built-in Input, 44,100 Hz, 16-bit stereo), running 12 seconds.
3. At about second 2, played the already installed generative score for eight seconds to exact-device Built-in Output → Mac headphone jack → user-wired Phonic MU802 2T Return. No Mac system audio default was changed.
4. Recorded 529,200 stereo frames in the 12-second WAV, recording exit 0; playback submitted 352,800 stereo frames, returned 8/8 buffers, playback exit 0. Neither device driver reported an error.
5. Measured **AC** RMS of every one-second segment in the saved WAV (subtracting DC, not mistaking a power-up transient for music). Seconds 3–9 stayed at approximately -77 dBFS in both channels, indistinguishable from seconds 10–11 after the playback ended. Mean linear AC RMS of playback segments 3–9: 4.5601 PCM counts; post-playback baseline 10–11: 4.5012 PCM counts. Ratio **+0.11 dB**. Thus **no materially detectable returned program audio**.
6. The WAV exists privately on the silver Mac at `~/Music/Fountain Coach/Die Maschine/Recordings/fc-actual-phonic-return-test-20261025-110646.wav`; do not commit that user's audio into public Git.

## What this proves, and what it does not

PROVEN: both Sierra analog ports are recognized; exact Core Audio programs perform genuine real-time playback and input recording. Input is not synthetic/generated. A WAV exists and was measured. The Blackmagic Thunderbolt-only setup and locked video output remained undisturbed.

NOT PROVEN: the Mac's electrical headphone signal reached Phonic 2T Return, the Phonic sends its 2T Return to the Main L/R bus, REC OUT is actually feeding Mac Line In, or the disconnected Blackmagic device has itself captured any audio/video. The sample increase of +0.11 dB is not evidence of signal return.

## Physical routing hypothesis requiring only one bounded action if desired

Phonic MU802's 2T Return is **not automatically main-mixed**. Manufacturer manual says the upper `TO MAIN L-R` button routes 2T Return into the main mix; the lower `TO CTRL RM` routes it to monitor/phones instead. REC OUT is derived from the main mix. A missing upper-button route is a likely *hypothesis*, not a proven physical switch state. The control's pressed position cannot be unambiguously determined from the photograph.

Avoid repeated speculative rewiring, HDMI changes, claims that recording succeeded as loopback, and asking user to watch LEDs. If the user engages the upper `TO MAIN L-R` switch, rerun the **same automatic 12-second record + 8-second playback comparison**, evaluating AC RMS and, if non-silent, matching to the known emitted score. Keep digital playback quiet and avoid enabling software input monitoring to prevent feedback.

Reference: Phonic MU802 manual's 2T Return Controls / Main L/R and REC OUT sections.
