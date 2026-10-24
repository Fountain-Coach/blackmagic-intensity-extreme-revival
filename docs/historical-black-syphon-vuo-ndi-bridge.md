# Historical Black Syphon → Vuo → NDI bridge

**Inspection:** 24 October 2026, original Intel Mac mini running macOS Sierra 10.12.6.

## Recovered implementation

The original VIDVOX **Black Syphon r4** application is still present:

`/Applications/Black Syphon.app`

Its bundled `VVBlackmagicKit.framework` directly interfaces with the installed Blackmagic DeckLink runtime and exposes:

- `VVBMInput` for Blackmagic capture
- `setInputModeInUse:allowInputDetect:`
- `deckLinkDetectCapable`
- `allowHotSwap`, `hotSwapEmptyFrameCount`
- `VVBMInputCallback::VideoInputFormatChanged`
- `processDeckLinkVideoFrame:audioPacket:` and asynchronous processing
- the Syphon framework for downstream interapplication GPU frame sharing

This **does not** replace the Blackmagic hardware driver. Syphon is the bridge *after* actual frame acquisition. An important diagnostic question is whether the old VVBlackmagicKit input acquisition, mode detection and hot-swap behavior succeeds where our direct implementation currently receives only `no-source` callbacks.

The old user-owned Vuo composition still exists at:

`/Users/bene/Desktop/ReceiveImages.vuo`

Its Graphviz-like Vuo source (last saved with Vuo 2.4.1 in May 2022) connects:

```text
vuo.syphon.receive
  serverDescription.serverName = "Intensity Extreme"
  serverDescription.useWildcard = true
  receivedImage
      |
      v
vuo.ndi.send.sendImage
```

The original node graph therefore routes **Syphon Intensity Extreme → NDI video**. No image transformation intervenes. The currently installed `vuo-2.0.0-composition-examples` is older than the saved composition; executability of the composition has **not** been demonstrated.

Another installed element is **Syphon Recorder**, whose old user preferences select the QuickTime `2vuy` codec. This is additional configuration evidence, **not** proof of prior physical capture.

## What the live legacy app proved

We temporarily quit the idle Fountain Video window to avoid two competing capture clients, then opened the installed **Black Syphon r4**. The UI showed **Intensity Extreme** under *Potential Inputs* and *Potential Outputs*, with black preview panels and an **empty input-options panel**. Per VIDVOX documentation, selecting an input reveals its capture options and activates publishing.

Our independently authored [read-only Syphon directory witness](../tools/sierra/syphon-server-witness.m) found:

```text
stage=legacy_syphon result=enumerated server_count=0
```

This means **no Syphon server was advertised to the witness in that user session at the time**. It does *not* prove that the Nikon failed; Black Syphon's input was not established as selected/active. The original UI cannot be operated remotely through AppleScript's System Events because the SSH-launched `osascript` has no Accessibility permission (`-1728`). We did not override macOS permissions or fabricate a capture result.

## Exact comparison with Fountain Video

| Boundary | Legacy route | Native Fountain Video |
| --- | --- | --- |
| Hardware transport | Existing Desktop Video 10.4 driver, not Syphon | Same installed driver |
| Frame acquisition | `VVBlackmagicKit/VVBMInput` | `DeckLinkAPI/IDeckLinkInput` |
| Mode negotiation | User-selected mode; optional *Detect Input Mode* on supported devices; optional continuous detection/hot-swap | Bounded serial six-format Nikon scanner |
| Processing | Asynchronous capture and GPU frame sharing | CPU YUV to RGBA for live preview |
| Downstream | Syphon server `Intensity Extreme` → Vuo → NDI | Internal preview, local PNG and MOV |
| Proven live video now | **No** — old bridge not activated | **No** — all Nikon scans reported no signal |

Official reference: [VIDVOX BlackSyphon r4 user guide](https://docs.vidvox.net/blacksyphon/), which documents *Detect Input Mode*, *Continuous Detect*, capture mode and single-client limitations. Note that features are available **when supported by the device**; we have not proven that the Intensity Extreme supports automatic hardware format detection.

## Engineering conclusion

Recover the historical method as a **compatibility baseline**, not as an unexamined permanent Syphon dependency. The repo's source-owned, platform-neutral video abstraction remains the intended publishing pipeline.

1. In legacy Black Syphon, activate *Potential Inputs → Intensity Extreme* and inspect the available *Mode / Detect Input Mode / Continuous Detect* settings. Only an active selected source can prove or disprove the old capture route.
2. When a real frame is present, use the read-only directory witness to confirm publication of the `Intensity Extreme` Syphon server, and optionally Vuo/Recorder to observe frame delivery. Publication without frame content is not accepted as a capture success.
3. Compare the successful VVBlackmagicKit mode-selection/format-change semantics with Fountain Video, and implement the corresponding **device capability-aware** behavior in our owned application.
4. Preserve the original driver and isolated crashing CoreMediaIO plug-in. Avoid replacing the kernel driver solely because the current video format remains unverified.

**No Nikon settings, Blackmagic driver, system boot preferences or MacBook display settings were changed during this investigation.**
