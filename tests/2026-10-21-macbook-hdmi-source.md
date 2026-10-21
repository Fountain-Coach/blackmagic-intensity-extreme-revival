# MacBook HDMI source and proposed hardware self-loop — 21 October 2026

**Provenance:** User-supplied physical photographs and direct diagnostic results on the two user-authorized Mac computers. No photographs, serial numbers, or MacBook-screen contents are published here.

## Physical connections observed

- Intensity Extreme HDMI **IN** is populated; HDMI **OUT** is empty.
- The Intensity Extreme Thunderbolt port is connected to the legacy Mac mini through a Thunderbolt dock; the capture backend remains Desktop Video 10.4 on Sierra.
- The contemporary Apple Silicon MacBook has a USB-C HDMI hub which is supplying the Blackmagic's HDMI input.
- The MacBook's `SPDisplaysDataType` explicitly identifies the external display **BMD HDMI**, **1920x1080 at 30 Hz**, active and mirrored with the built-in display.
- The legacy Mac mini instead shows a generic **1920x1080 at 60 Hz** display, establishing that the current source differs from the earlier silver-Mac HDMI-to-Blackmagic setup.

A MacBook dialog asking to start synchronization was reported by the owner and confirmed in the interface, but its triggering service is **not established**. It cannot be treated as evidence of video-capture sync.

## Receiver with the MacBook as source

Read-only DeckLink configuration: input connector remains HDMI (`0x2`).

```text
stage=requested_video_mode code=0x48703330
stage=set_callback status=0x00000000
stage=enable_input status=0x00000000
stage=start_streams status=0x00000000
stage=queue_frames rc=0x00000000 queued=0
stage=status_interface rc=0x00000000 present=1
stage=signal_lock rc=0x00000000 locked=0 detected_mode_rc=0x80000001 mode=0x0
stage=stop_streams status=0x00000000
stage=remove_callback status=0x00000000
stage=disable_input status=0x00000000
stage=counts frames=0 valid=0 no_source=0 format_changes=0 observer_refcount=1
stage=complete exit=0
```

The second Mac source thus also yields **no receiver lock**, despite being visible as an HDMI display endpoint. No captured frame has yet been demonstrated.

## MacBook HDMI EDID diagnostics

A locally decoded 256-byte EDID block for **BMD HDMI** has one CTA extension and valid checksums; the extension advertises both YCbCr **4:2:2** and **4:4:4** support. This is a sink *capability advertisement*, not a reliable reading of the MacBook's current output encoding. The original Intensity Extreme's published HDMI sampling specification is YUV 4:2:2. We must not assert that the MacBook presently sends RGB without direct evidence. HDCP and signal-negotiation behavior also remain unresolved.

Reference: [original-model hardware listing](https://www.bhphotovideo.com/c/product/824118-REG/Blackmagic_Design_BINTSTBEXT_Intensity_Extreme_HDMI_and.html).

## Our own 1080p25 color bars and concurrent capture API test

The new [bounded color-bar output program](../src/decklink-colorbars-burst.cpp) filled a 1920x1080 8-bit YUV frame and submitted it **125 times** through `DisplayVideoFrameSync` over an approximately five-second burst. Output enable, all 125 submissions, and output disable returned success. While output was active, our 1080p25 input observer successfully installed a callback, enabled the input, started and stopped streaming, and disabled the input. It observed **zero frames**, as the HDMI cable was still plugged into the MacBook rather than the Blackmagic's HDMI OUT.

This tests simultaneous use of the two SDK interfaces but **does not verify full-duplex video transfer**. No receiver lock should be inferred from these return codes alone.

## Next experiment: controlled self-loop

The current physical cable has one end at the Intensity Extreme **HDMI IN** and the other end at the MacBook USB-C hub. Move only the hub-side HDMI plug into the **empty HDMI OUT** socket on the Intensity Extreme, leaving the existing input end untouched. This yields:

```text
Blackmagic HDMI OUT -- (same HDMI cable) --> Blackmagic HDMI IN
                    ^                       |
                    |                       v
            legacy host DeckLink OUTPUT + INPUT APIs
```

This arrangement removes the MacBook, its HDMI color encoding, and any potentially protected video source from the video path. Our entirely generated YUV test pattern is not protected media.

After the physical loop is verified, run the same bounded generated-output burst concurrently with the matching 1080p25 capture observer, and report lock and frame callback counts. Capture buffers must not retain private media; the test pattern itself is generated.

**Caveat:** The legacy hardware's actual simultaneous hardware playback/capture support is not yet established, despite successful concurrent API lifecycle calls. If it cannot operate in full duplex, use a separate unprotected HDMI camera or pattern generator as a deterministic source.

Do not flash firmware, bypass HDCP, or change operating-system drivers as part of this experiment.
