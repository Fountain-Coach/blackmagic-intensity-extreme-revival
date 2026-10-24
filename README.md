# Blackmagic Intensity Extreme Revival

**Current engineering decision — Sierra physical host + Linux Docker guest (24 October 2026).** The original silver Macmini5,3 continues booting **macOS Sierra 10.12.6**, maintaining the only verified Thunderbolt 1 → Comdis dock → Blackmagic Intensity Extreme (PCI 0xBDBD:0xA124) connection. The real video instrument is to be consumed by **Linux/Docker**, but Sierra continues to own the physical PCI/Thunderbolt endpoint. We have booted an Alpine 3.24.3 Linux guest on the silver using source-built xhyve, run **Docker Engine 29.9.2**, executed `hello-world`, restarted the guest using persistent Docker data and run the actual Fountain Video browser-scratch receiver in a restricted container with **18 passing receiver tests**. The VM's virtual network is NOT yet authorized; physical HDMI input remains unverified. No Ubuntu host replacement, Sierra driver upgrade, reboot or firmware change. [Full witnessed silver Docker acceptance](tests/2026-10-24-silver-sierra-docker-runtime.md) · [Sierra source-neutral capture/guest boundary](docs/sierra-docker-capture-boundary.md) · [Explicit source-owned guest launcher](scripts/silver-video-docker-guest.sh). Independent current-Ubuntu PCI/driver emulation is a *separate optional research lane* and has not gained access to the physical A124.

**Latest serial-transport outcome (24 October 2026):** A secondary xhyve virtual serial interface passed small round-trip messages and five software-only transport tests, but its **physical silver-host media trial failed**. A real 718-byte synthetic JPEG sent to the Sierra loopback proxy was rejected with HTTP **503**, the Linux Docker scratch receiver retained **zero frames**, and direct binary/ASCII payloads lost bytes across the emulated UART. We have **not** connected Sierra video to the silver's Linux guest. The experimental serial containers and proxy were stopped; the guest was shut down without changing the original video app's working Ubuntu target. **Do not enable the COM2 transport as a production video lane.** The original Ubuntu receiver continues working. [Live serial media rejection and evidence](tests/2026-10-24-silver-com2-real-jpeg-rejected.md). The real HDMI capture path remains **NOT ACCEPTED**.

**Follow-up serial diagnosis:** The live Linux guest had an Alpine `ttyS1` getty concurrently reading the same device as the Docker media bridge. We witnessed its login process interpreting the `FCSV1` JPEG header as a username. After disabling this competing login in the disposable guest, an independently checked **512-byte binary SHA-256 matched exactly** across Sierra → xhyve → Linux. That changes the diagnosis from presumed inherent UART byte loss to a **competing-reader defect**, now isolated. A full authenticated JPEG delivery to Docker is still **unaccepted**, and the prior bridge container hung on shutdown; the guest virtual disk requires offline integrity verification before reuse. [Exact evidence and guest ownership guard](tests/2026-10-24-sierra-getty-com2-exclusive-witness.md) · [Alpine guest preflight script](scripts/alpine-guest-exclusive-com2.sh).

**Accepted Sierra → local Linux Docker transport (24 October 2026):** With Alpine's conflicting ttyS1 login disabled, a private authenticated **718-byte synthetic JPEG** crossed the silver Sierra Mac → xhyve COM2 → Linux Docker sidecar → original Fountain Video scratch receiver. The Sierra HTTP response was **201 Accepted** (`seq: 0`, 6.216 seconds); the JPEG fetched independently inside Docker matched the Sierra source's exact SHA-256 `4982c239c16e03886c60eff9fb74d52371dcd9dee6a7dc3285c277841c4f6a70`. This is **real transport acceptance**, not mock PTY behavior, but is **too slow for real-time video** and emphatically **does not prove physical Blackmagic HDMI capture**. [Real-machine acceptance witness](tests/2026-10-24-silver-real-jpeg-to-local-docker-accepted.md).

**Software repair is hardware sustainability.**

A video-capture device does not stop being useful simply because its manufacturer stops supporting the software around it. This independent Fountain Coach project investigates whether a legacy **Blackmagic Intensity Extreme** can be brought back into service with documented diagnostics, reproducible reverse engineering, and eventually an openly implemented hardware interface.

Inspired by the practical spirit of a *Repair Café*: understand what is broken, share what we learn, and make functioning equipment useful for longer.

> **Current status — native capture/playback APIs recovered; HDMI receiver not locked.** Our self-authored programs enumerate the device, negotiate input and output modes, and submit a generated playback frame through the installed legacy driver. Earlier capture tests with external HDMI sources reported **no input signal lock** at 1080p30 and no valid frames; a temporary 720p60 test also failed. On 23 October, a new bounded 1080p30 test received **83 video callbacks, but every frame was explicitly flagged as having no input source** (zero valid frames, signal lock false). Source signal presence during this retest was not independently verified; missing lock is not by itself proof of failed receiver hardware. With Blackmagic HDMI OUT physically looped into HDMI IN, native tests established **half-duplex behavior**: whichever capture/playback mode was enabled second was rejected. Physical signal transfer and an independent replacement driver remain unverified.

**Official Sierra driver audit (24 October 2026):** An authentic Apple-signed Blackmagic Desktop Video **10.11.2** Mac installer has been acquired and verified OFFLINE. Its release notes list **Sierra 10.12**, the actual extracted KEXT precisely matches **pcibdbd,a124** (Intensity Extreme), and the KEXT itself passes code-signature verification. The package **removes and replaces** the active 10.4 driver/framework, installs firmware-updater services, requires root and a reboot. **No installation was performed**, because this Sierra Mac has no independently boot-tested fallback. The original 10.4 components and official 10.11.2 archive are preserved privately outside Git. **Actual HDMI input remains unverified.** [Signed installer and recovery/no-go audit](tests/2026-10-24-official-desktop-video-sierra-package-audit.md) · [offline package checker](scripts/audit-offline-desktop-video.py).

**Sierra recovery / exact 10.4 rollback readiness (24 October 2026):** The actual Macmini5,3 Recovery HD contains a checksum-valid Sierra 10.12.6 BaseSystem, supported model metadata and blessed boot configuration; the [read-only recovery witness](scripts/sierra-recovery-readonly-witness.sh) passes. Apple's full Sierra 10.12 installer image was independently SHA-256 verified on Ubuntu and the silver Mac, and its embedded installer package passed **Apple Software** signature verification on Sierra. The original Blackmagic **Desktop Video 10.4 official signed installer** has been recovered; its KEXT and DeckLinkAPI hashes are **identical** to the actual binaries on Sierra. All sources and recovery files are privately archived on Ubuntu outside Git. **Physical reboot/recovery and a full-system rollback are NOT tested**: external dedicated Sierra installer and separate full-system backup media are still required, with a local console. The destructive driver/firmware upgrade remains **NO-GO**. [Detailed recovery/rollback evidence](tests/2026-10-24-sierra-recovery-readiness-exact-rollback.md).

**Independent-driver progress (23 October 2026):** our own [I/O Kit PCI inventory](src/intensity-pci-inventory.c) detected the actual Intensity Extreme on macOS Sierra, and a separate [read-only resource probe](src/intensity-pci-resources.c) reported two 1 MiB PCI memory-resource descriptors. A [passive Sierra kernel-driver skeleton](driver/sierra/passive/) builds from a clean Git checkout and passes offline linkage diagnostics, but has **not been loaded or matched to the device**; the original kernel driver remains active. This is driver-development progress, not captured video. [Sierra tests](tests/2026-10-23-independent-driver-sierra-offline.md) · [PCI resource evidence](tests/2026-10-23-pci-resource-layout.md).

## Full software video laboratory — downstream media simulation

**No physical source required.** The Nikon D5200 may remain switched off. Real HDMI inputs and outputs, Syphon, DeckLinkAPI and proprietary drivers are **not dependencies** of the video instrument's simulated feature contract.

The [full software video laboratory](docs/full-software-video-laboratory.md) models a camera, two separate half-duplex video instruments, virtual video playback into a virtual receiver, 16 video formats, deterministic full-resolution YUV frames, stereo PCM audio, scheduled output buffers, rational timestamps, format hot-swap, no-signal, protected-input blocking, dropped frames, fault recovery and a bounded raw video/audio archive.

**Open the live software lab on the private WLAN:** http://192.168.8.50:8272/lab

The real Sierra [Fountain Video.app](apps/fountain-video-sierra/README.md) now starts in **synthetic video mode** rather than scanning Blackmagic hardware and includes a **Software lab…** button to open the full simulation. Optional real capture remains a separate backend.

Reproduce the laboratory with Python 3.10+ standard library only:

    python3 -m unittest discover -s simulator/tests -v
    python3 -m simulator.lab_server --host 127.0.0.1 --port 8272

The original minimal simulator remains separately available through the earlier 'simulator.server' command on port 8172. All generated media and HTTP outputs are explicitly marked **SYNTHETIC**; none establishes actual HDMI capture.

## Legacy driver failure affecting other macOS applications

Our tests exposed a reproducible legacy-Desktop-Video integration defect: opening **App Store 2.2.1 on Sierra** crashes inside Blackmagic's system-wide CoreMediaIO device-discovery plugin and DeckLinkAPI. Blackmagic's hardware helper also crashes at nearly the same time. We have [documented the four matching crash stacks](tests/2026-10-21-app-store-blackmagic-crash.md), the limits of current evidence, and a reversible plugin-isolation experiment. **The App Store startup crash is mitigated by isolating the old CoreMediaIO plugin. An [experimental four-byte patch](tests/2026-10-21-cmio-offline-repair-candidate.md) is structurally loadable, but a bounded system-level trial still enumerated zero CoreMediaIO devices; the proprietary bug is not considered fixed.**

## What we want to repair

The tested setup includes:
- **Blackmagic Intensity Extreme**, PCI vendor **0xBDBD**, device **0xA124**.
- **Comdis/i-tec THUNDERDOCK2** between the capture device and host.
- **2011 Mac mini**, macOS **10.12.6 Sierra**, with **Desktop Video 10.4** installed.

The device enumerates over Thunderbolt and PCI Express. macOS attaches the Blackmagic driver, including audio-related components. **Our independently written native test can now enumerate the device, discover video formats, and temporarily enable/disable input and output modes.** Following the verified CoreMediaIO isolation, Media Express now launches its interface and acquires a device resource (busy state `0x2`, returning to `0x0` when closed); actual captured frames remain unverified. [Repeatable regression results](tests/2026-10-21-post-isolation-driver-regression.md) distinguish those facts.

## The repair approach

1. **Observe before changing anything.** Record model IDs, software versions, connection topology, failures, and exact artifact hashes.
2. **Understand the working boundaries.** Separate PCI enumeration, driver attachment, firmware readiness, device usability, video DMA, and actual capture.
3. **Reconstruct behavior from evidence.** Analyze an authorized local copy of the legacy kernel extension using [Reverse Engineer Anything (REA)](https://github.com/morluto/rea) and [Ghidra](https://github.com/NationalSecurityAgency/ghidra).
4. **Implement a minimal diagnostic.** First determine whether a supported user client can report device, firmware and readiness state without modifying hardware.
5. **Reclaim the useful interface.** Build a portable video instrument with replaceable host backends. Examine independent driver implementation as a separate, testable objective.
6. **Measure what works.** Reproduce successful frames, timestamps, audio synchronization, and controlled output—rather than promising compatibility by assertion.

**No Syphon dependency.** The forward architecture is intended for current macOS, Linux, and iPadOS consumers, although a low-level Thunderbolt/PCI host is still needed for this particular device.

## Evidence so far

| Layer | Observed result |
| --- | --- |
| Thunderbolt and PCI Express | Enumerated; vendor `0xBDBD`, device `0xA124` |
| macOS kernel driver | DeckLink driver attached and audio engines registered |
| Vendor capture application | Initially reported no hardware; after CMIO plugin isolation Media Express launches and acquires/releases device resource, but no physical frame captured yet |
| Binary inspection | 64-bit Intel Mach-O kernel extension with readable C++ symbols |
| REA + Ghidra | First analysis successful, including readiness and firmware callbacks |
| Native DeckLink program | Device enumerated as Intensity Extreme; both video interfaces and 16 modes each discovered |
| Direct video output format queries | 64 calls complete without crashing; 8-bit YUV supported in 16 modes; 10-bit YUV supported in five standard-definition modes |
| System CoreMediaIO discovery with plugin isolated | Query completes normally; no video devices advertised through DAL plugin |
| Physical connectors | HDMI, component, composite, S-Video masks returned for input and output |
| Video mode activation | 1080p25, 8-bit YUV input and 1080p25 output enabled and disabled successfully |
| Generated output frame submission | 1920×1080 8-bit YUV frame created and accepted by synchronous display API; physical sink not verified |
| HDMI self-loop signal | Mac mini display identifies BMD HDMI at 1080p30; legacy configuration selects HDMI input (`0x2`) |
| HDMI receiver signal lock | **False during active 1080p30 capture** with an external computer source |
| Simultaneous capture and playback | **Rejected** whichever direction was enabled second (half-duplex); generated HDMI self-loop cannot function |
| Input stream callback | Earlier 1080p25/30 and 720p60 tests produced no valid frames. The 23 October 1080p30 retest delivered **83 no-source callbacks**, **0 valid frames**, no signal lock; clean stream teardown. |
| Input frame capture / physical output verification | **Not yet demonstrated** |
| Independent driver | Source-owned PCI inventory probe passed on Sierra; passive IOKit kernel bundle builds and passes **offline linkage validation**, but has **not been installed, loaded, or used for actual capture**. |

The [confirmed half-duplex loopback result](tests/2026-10-21-half-duplex-loopback.md) supersedes the [earlier proposed MacBook HDMI self-loop](tests/2026-10-21-macbook-hdmi-source.md). The earlier report documents the MacBook HDMI source and the now-disproven self-loop hypothesis. The [hardware capability inventory](docs/hardware-capabilities.md) tracks every documented video, audio and conversion function. The [native enumeration probe](src/decklink-enumerate.cpp), [single-frame output experiment](src/decklink-output-frame.cpp), and [bounded input observer](src/decklink-input-observer.cpp) demonstrate the progressively recovered interface. The [dated enumeration](tests/2026-10-21-native-enumeration.md), [output](tests/2026-10-21-single-frame-output.md), [input](tests/2026-10-21-input-callback.md), and [HDMI loopback diagnostics](tests/2026-10-21-hdmi-self-loop.md) logs separate successful API calls from unverified physical signals. The [investigation log](docs/investigation.md) records the reverse-engineering evidence. The complete copyrighted vendor binaries, firmware and their decompiler output are **not distributed here**.

## Independent driver milestone — 23 October 2026

We restored access to the physical Sierra hardware host and tested our source-owned [read-only PCI inventory](src/intensity-pci-inventory.c) there: `IOPCIDevice` `0xBDBD:0xA124` detected, one match, clean exit. The first [passive IOKit kernel driver](driver/sierra/passive/) now builds *from a fresh repository checkout on Sierra* and passes Apple's `kextutil -n -t -z` **offline** dependency and structural validation. It has no automatic matching personality, is unsigned, and was **not loaded or installed**. Both original Blackmagic drivers remain in place. See the [Sierra driver smoke test](tests/2026-10-23-sierra-independent-driver-smoke.md) and [new HDMI no-source callback evidence](tests/2026-10-23-hdmi-no-source-callback-retest.md). A [hash-pinned, read-only model-dispatch inspector](scripts/inspect_private_model_dispatch.py), verified with four synthetic tests and an authorized private driver copy, identifies `0xA124` as **HDExtreme model code `0x36`**, not simply the generic PCI Express variant. The matching class's static initialization points to a candidate control mapping through the first PCI BAR (`0x10`); see [model-specific evidence](docs/a124-hdextreme-dispatch-2026-10-23.md) and [DMA protocol findings](docs/dma-protocol-evidence-2026-10-23.md). The independent hardware protocol, DMA, interrupts and physical capture remain unimplemented and unverified.

## Model-specific status decoder — 24 October 2026

We independently resolved the formerly indirect HDExtreme I/O Kit vtable calls using a [compile-only ABI verifier](driver/sierra/passive/verify-abi-layout.sh) on **the physical Sierra Mac mini**. The dispatch offsets `0x908` and `0x118` resolve to `IOPCIDevice::mapDeviceMemoryWithRegister` and `IOMemoryMap::getVirtualAddress`. The HDExtreme static initialization path maps BAR0 (register `0x10`), and its format detector points to a status candidate at **BAR0+`0x40`**. A separate [hash-pinned Mach-O virtual dispatch inspector](scripts/inspect_private_status_dispatch.py) verifies the HDExtreme `0x7a0` method slot resolves to inherited `HDProPCIExpress::ReadRegisterEndian` without publishing original binary bytes. The original code's `0xffffffff` invalid-response guard was identified.

Our [pure status classifier](src/intensity-input-status.c) and [11 synthetic C tests](tests/test-intensity-input-status.c) compiled and passed on both Sierra and Ubuntu. Twelve synthetic tests also pass across the two private-binary dispatch inspectors on Ubuntu. The separate [A124 firmware gate model](src/intensity-firmware-gate.c) passes six additional synthetic tests on both machines, and the hash-pinned private inspector now verifies the model's expected firmware-version byte **`0x0B`**. Model-specific static analysis traces its firmware version query to **BAR0+`0x224`**, while input status remains at **BAR0+`0x40`**; neither register has been read by our code. These are independent software test results, **not live BAR accesses, firmware readiness, signal lock, or captured video**. Read the exact [A124 status call-chain and evidence limits](docs/a124-input-status-call-chain-2026-10-24.md).

The [recovery inventory](tests/2026-10-24-sierra-recovery-inventory.md) found no configured Time Machine destination or verified alternative boot environment, and the `Fresh` volume contains existing user data. An actual PCI-driver takeover must remain blocked until a separate, tested rollback boot exists.

## Live capture preflight and driver-contract progress — 24 October 2026

The current MacBook and Sierra Mac mini both show a **U28E590** external display instead of the previously observed **BMD HDMI** HDMI sink. Therefore the earlier MacBook-to-Intensity cabling is no longer verified. The new [cross-version source preflight](scripts/hdmi-source-preflight.py) fails closed if no active BMD HDMI EDID and exact supported timing are present. The [bounded capture acceptance runner](scripts/accept-bmd-hdmi-capture.sh) executes the Sierra vendor-API observer only after that source preflight passes. Its first live invocation correctly stopped before remote capture (exit 2).

In a separate, deliberately bounded live vendor baseline, the improved [capture observer](src/decklink-input-observer.cpp) received **84 callbacks, all marked no-source**, with **zero valid frames** and no signal lock even after three seconds. It now collects a bounded first-valid-frame sample fingerprint and timestamp without storing source video, and validates a clean teardown. Five synthetic callback fixtures passed on Sierra and modern macOS; a strict acceptance parser passed twelve synthetic tests on Ubuntu.

Our independent [A124 register contract](src/intensity-register-contract.c) combines the model-specific firmware/version and video-status gates with an injectable read-only register callback. **Twelve mocked BAR0 contract tests passed on both Sierra and Ubuntu.** It has not been bound to physical PCI memory, and no independent capture driver has been loaded. The operating hardware still requires a verified recovery boot and an actual compatible unprotected input signal. See [complete 24 October test evidence](tests/2026-10-24-hdmi-source-gated-capture-and-read-contract.md) and [D4 physical acceptance](https://github.com/Fountain-Coach/blackmagic-intensity-extreme-revival/issues/8).

## HDMI streaming and browser scratching

**New safe physical HDMI diagnosis:** The silver Mac mini's BMD HDMI framebuffer has been independently read and hashed at 1920×1080/30; consecutive real OS frames changed. The [one-command Sierra physical preflight](tests/2026-10-24-hdmi-output-framebuffer-and-frame-lifetime-triage.md) also validates our DeckLink callback against a source-owned mock with sanitizers, and identifies the exact failing vendor frame-destructor release. It does **not** claim an accepted capture frame. The native app now protects the working synthetic/browser stream from accidental re-entry into the crashing Desktop Video 10.4 input path.

The [browser scratch instrument](docs/hdmi-to-browser-stream-and-scratch.md) now receives **real transported JPEG frames** from the native Sierra Fountain Video.app over the private LAN, retains up to 144 indexed frames, and supports drag-to-jog, reverse and forward shuttle, freeze, frame stepping, and return to live. A separate [full software lab](docs/full-software-video-laboratory.md) remains available.

**[Open Browser Scratch](http://192.168.8.50:8273/scratch)** (private WLAN).

The native source and transport are shared between synthetic video and the optional actual DeckLink callback. **The tested ingress is SYNTHETIC; no physical HDMI frame was captured.** The browser visibly labels the provenance and never claims hardware revival. The scratch implementation transmits a bounded 640×360 JPEG preview stream rather than full-frame-rate WebRTC or uncompressed HDMI. Tests passed on both Ubuntu and macOS, including eight real Chrome drag/keyboard/transport interactions. The original drivers remain untouched.

## Fountain Video — native Sierra application (24 October 2026)

**Fountain Video.app has been built and launched on the real 2011 Intel Mac mini.** This is a native AppKit application, rather than another Python-only video simulation or command-line diagnostic. It is installed in the Sierra account's personal Applications folder and communicates directly with the existing Desktop Video 10.4 runtime, bypassing the isolated crash-prone CoreMediaIO plug-in.

The window provides Blackmagic HDMI capture, automatic scanning of four common video modes, real-frame preview, PNG still export, QuickTime MOV recording, and a conspicuously identified synthetic video source. On Sierra, the application has passed its actual GUI launch, hardware discovery, synthetic movie-encoding and virtual-preview tests. An automatic four-format scan completed cleanly and reported no input signal. **No physical HDMI frame has yet been captured.** The MacBook is neither required nor changed as a test source.

The app uses only the installed legacy Blackmagic user-space API; the experimental independent PCI kernel driver remains offline. Real HDMI preview and recording paths exist but are not yet accepted against genuine frame data. [Source, build instructions, and test results](apps/fountain-video-sierra/README.md).

## Why this matters

Keeping functional equipment in use may reduce electronic waste and the embodied impacts of purchasing replacement devices. But *carbon savings are not assumed*. An old computer also consumes power, and reverse-engineering activity has a cost. Our [sustainability ledger](docs/sustainability.md) explains which data would justify a real claim.

We want this to be both an engineering project **and a practical public example of software-led repair**—a place where a repair volunteer, artist, researcher or systems architect can follow the evidence and try the next experiment.

## Contribute

Owners of similar devices can contribute sanitized hardware inventories, driver-version observations, error messages, independent technical notes and reproducible tests. See [CONTRIBUTING.md](CONTRIBUTING.md). Please **do not upload proprietary drivers, firmware images, private user data or copied vendor source**.

This is an independent community repair initiative, **not affiliated with or endorsed by Blackmagic Design**. All respective trademarks belong to their owners.

The original materials in this repository are MIT licensed (see [LICENSE](LICENSE)). Proprietary third-party software and hardware are not relicensed by this project.

**Fountain Coach** — *We make meaning executable.* Here, that includes making the meaning of repair demonstrable.
