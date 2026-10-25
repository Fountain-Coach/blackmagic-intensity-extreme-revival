# Fountain Video Instrument: independently generated Syphon frames and physical HDMI playback

**Witness:** 25 October 2026, original silver Macmini5,3 (Sierra 10.12.6), original Intensity Extreme PCI identity 0xBDBD:A124 over original Thunderbolt.  
**Physical wiring:** silver Mac HDMI OUT → original Intensity HDMI IN; original Intensity HDMI OUT → Panasonic HDMI IN as connected by owner; no changes during this implementation.

## Why this test exists

The owner correctly rejected a ChatGPT answer that displayed a **blank generic image-reference panel** while making claims about generated images. That reference was neither a real screenshot nor visual evidence of Blackmagic output. The replacement is actual source-controlled software and a reproducible screenshot **captured from the real Fountain application window on Sierra**.

[Actual Sierra Fountain Video Instrument image, captured from the running application](evidence/2026-10-25-sierra-fountain-video-instrument.png).

## Source-owned visual instrument

- [Pure owned image kernel](../src/fountain-video-score.h): **Die Maschine** produces a time-varying geometric score using integer-only pixel generation in both 32-bit RGBA and 8-bit limited-range Rec.601 UYVY (the original DeckLink 2vuy layout).
- [Owned Syphon publisher](../src/fountain-syphon-instrument.m): native Sierra Objective-C/OpenGL application; independently opens its own OpenGL context, renders into an owned texture, publishes a SyphonServer named **Fountain Die Maschine**, and displays the image in a normal Sierra window. Does **not** use the Black Syphon application or its capture logic.
- The actual already-installed [Syphon.framework](https://github.com/Syphon/Syphon-Framework) in the Sierra Syphon Recorder bundle was used as an interprocess GPU image transport. It retains its own original permissive license. Syphon itself is third-party; the Fountain renderer and publisher are our own. The operating system and OpenGL are Apple dependencies.
- [Independent Syphon client witness](../tools/sierra/fountain-syphon-frame-witness.m): a second process discovers the **Fountain Die Maschine** server, attaches to the texture in a separate OpenGL context and reads real pixels back using **an FBO + glReadPixels** on the received IOSurface texture.
- An initial independent witness used glGetTexImage and produced identical empty pixels; this was explicitly **not accepted**. Changing to FBO glReadPixels verified both nonzero pixels and changing frame CRCs.

## Independent Syphon real-pixel evidence

```text
stage=syphon_client result=connected source=Fountain_Die_Maschine
stage=syphon_frame sample=1 pixels=960x540 crc=0x14b433bf sampled_nonzero=21378
stage=syphon_frame sample=2 pixels=960x540 crc=0x2ef02056 sampled_nonzero=21378
stage=syphon_client result=PASS real_pixels_read=1 frame_variation=1 capture_source=generated
```

**Installed application** subsequently repeated acceptance with different frames:

```text
stage=syphon_client result=connected source=Fountain_Die_Maschine
stage=syphon_frame sample=1 pixels=960x540 crc=0xf65d2eb4 sampled_nonzero=21378
stage=syphon_frame sample=2 pixels=960x540 crc=0x5b4fea68 sampled_nonzero=21378
stage=syphon_client result=PASS real_pixels_read=1 frame_variation=1 capture_source=generated
stage=installed_system_e2e witness_exit=0 publisher_exit=0
stage=syphon result=bounded_complete submitted=125 synthetic_only=1 capture_calls=0
```

This verifies **interprocess GPU pixel transport and time-varying video**, not physical capture from HDMI.

## Independent physical Blackmagic HDMI output adapter

[Source-owned adapter](../src/fountain-video-output.cpp) uses the *same image kernel* to fill a DeckLink output frame with changing UYVY pixels. It does not open the input/capture interface. It currently uses the original installed Desktop Video 10.4 DeckLinkAPI/runtime; this is **not yet a proprietary-driver replacement**.

Physical bounded test, original Intel Sierra + original Intensity Extreme:

```text
stage=device_model status=ok model=Intensity Extreme
stage=attribute name=video_output_connections status=0x00000000 value=0x3a
stage=attribute name=device_busy_state status=0x00000000 value=0x0
stage=mode interface=output code=0x68703630 width=1280 height=720 rate=60000/1000 name=720p60
stage=output_enable mode=720p60 size=1280x720 fps=60 rc=0x00000000
stage=frame_create rc=0x00000000 width=1280 height=720 bytes=1843200
stage=physical_score progress_sec=1 frame_submissions=60 generated_poses=10 frame_crc=0x2955b1cd
stage=physical_score progress_sec=8 frame_submissions=480 generated_poses=80 frame_crc=0x44894e03
stage=physical_score result=DONE mode=720p60 submitted=480 attempted=480 generated_poses=80 elapsed_s=8.004 hdmi_wire_image=UNVERIFIED
stage=disable_output rc=0x00000000
stage=complete exit=0 capture_enabled=0 input_hdmi_replugged=0 playback_only=1
```

The config-only [existing DeckLink inspector](../src/decklink-configuration-inspect.cpp) also reported **selected output connection mask 0xA**, including the HDMI bit 0x2, and input connection 0x2 (HDMI). The physical device was idle before and after the run. The Intel Mac display remained BMD HDMI 1920×1080@30 and its selected colour mode remained automatic YCbCr444.

**Critical distinction:** The DeckLink driver accepted 480 frames and the HDMI output connector is selected in its configuration; neither API status alone proves **that the Panasonic decoded a stable picture**. The Panasonic previously repeatedly powered/synchronised on and off while no Black Syphon input was selected. There is **no physical optical witness during this eight-second output test**. The physical output/image gate remains OPEN.

## Repeatable build, installation and QA

- [Source-owned installer](../tools/sierra/install-fountain-video-instrument.sh) compiles all applications using the actual Sierra Clang and local installed Syphon headers. It checks original host type, reuses only the existing local Syphon framework, creates an ordinary app bundle with its licensing/provenance note, and installs the hardware-output CLI and read-only Syphon pixel witness. No driver, SIP, OS settings, firmware or cabling changes.
- Installed Sierra app: `~/Applications/Fountain Video Instrument.app`.
- Installed HDMI adapter: `~/Library/Application Support/Fountain Coach/Video Instrument/bin/fountain-video-output`.
- Installed pixel-readback witness: `~/Library/Application Support/Fountain Coach/Video Instrument/bin/FountainSyphonWitness.app`.
- Pure [video score test](test_fountain_video_score.c) passed with **matching deterministic fingerprints** on the silver Mac Sierra, modern macOS and Ubuntu:
```text
stage=video_score_test result=PASS frames_differ=1 deterministic=1 valid_2vuy=1 rgbo_opaque=1 crc_frame0=0xac0ab377 crc_frame91=0xc97301af
```
- The installed app and separate installed witness passed their subsequent real Sierra interprocess frame test; both processes returned success.

## Acceptance matrix

| Boundary | Acceptance |
| --- | --- |
| Independently owned procedural visual image kernel | **PASS** |
| Deterministic pixels on Sierra/macOS/Ubuntu | **PASS** |
| Actual generated Sierra window image | **PASS**, screenshot linked |
| Syphon server published by our own app | **PASS** |
| Real changing pixels received in separate Syphon process | **PASS** |
| Original Intensity Extreme enumerates supported HDMI output mode | **PASS** |
| Physical DeckLink output enabled, 480 varied frame submissions, disabled | **PASS** at original vendor API |
| Exact HDMI output connector mask includes HDMI | **PASS**, config 0xA |
| Physical HDMI waveform/receiver lock at Panasonic | **NOT VERIFIED** |
| Panasonic visibly shows generated imagery | **NOT VERIFIED** |
| Blackmagic HDMI input capture | **FAIL / NO LOCK** from earlier independent experiments |
| Replacement hardware kernel driver independent of DeckLink 10.4 | **NOT IMPLEMENTED** |

## Next value-producing work

**Don't replace the working native image engine with decorative mockups or third-party GUI automation.** Preserve the verified shared image kernel across GPU publication and hardware playback. The next hardware-specific acceptance is to establish the Panasonic output signal's actual electrical/sink state. Investigation of HDMI output state/Hot Plug Detect and the native Thunderbolt/PCI user-client contract should be read-only until register semantics and rollback are proven. A replacement driver should be developed against independent virtual/device contracts and transferred to real hardware only after witnessed, safe I/O ownership. Do not uninstall the original driver without an actual recovery environment.

Until an optical/receiver witness is available, do not label API frame acceptance as visible video output.
