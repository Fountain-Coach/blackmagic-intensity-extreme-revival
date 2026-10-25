# A124 independent driver: start-to-finish plan

Date: 25 October 2026
Status: ENGINEERING PLAN, not a working replacement driver
Owner: Fountain Coach, repository Fountain-Coach/blackmagic-intensity-extreme-revival
Governance: FCIS Goal Fidelity and Non-Invention, Chapter 07; Git source authority; local independent witnesses; no GitHub CI.

## 1. Correct the transport model

Thunderbolt was Apple-first, not Apple-only. Intel introduced it in 2011 with Apple as first adopter, and it subsequently reached PCs and Linux. It tunnels PCI Express as well as DisplayPort; the host's operating-system Thunderbolt infrastructure manages the transport. Our project needs a driver for the original Blackmagic PCIe endpoint (vendor BDBD, device A124), not an original implementation of Thunderbolt or a Black Syphon GUI.

Mac-first physical driver engineering is the shortest evidenced route: the A124 already enumerates on the silver Macmini5,3 running Sierra, and the original dock/Thunderbolt wiring is intact. The Ubuntu Macmini8,1 has Thunderbolt controllers but does not have A124 physically connected. Thus Ubuntu is the development, protocol analysis, test and possible later Linux driver environment. Replugging to Ubuntu through an unproven TB3-to-TB1 adapter/power path is NOT a mandatory prerequisite or the default execution lane. This corrects the earlier Ubuntu-first hardware decision, while retaining all useful Ubuntu work.

The correct decomposition is:
- Existing OS Thunderbolt and standard PCIe enumeration
- OS-specific PCIe ownership, mapped registers, DMA and interrupts
- Fountain-owned model-specific A124 protocol implementation
- Fountain-owned image/audio frame service and control
- Existing Die Maschine, Teatro, Reframe, Syphon and capture/playback clients

The PCIe protocol core should be portable; PCIe attachment/binding is necessarily OS-specific.

## 2. Fixed owner outcomes and scope

The owner's goals must not be substituted by new infrastructure work.

Outcome S — STOP, FIRST PRIORITY: with existing cables attached, end the Panasonic's repeated on/off switching by a real software-controlled Intensity HDMI OUT shutdown. Prove the Panasonic or independent HDMI receiver stays stable/off. Stopping a process, device busy=0 or accepting a configuration API call is not physical STOP acceptance.

Outcome P — PLAY: on a future explicit instruction to enable output, show actual changing Fountain-created images on the Panasonic from original Intensity HDMI OUT, with stable resolution/timing and a reliable hardware Stop. Hardware API frame submission alone is not an optical image witness.

Outcome C — CAPTURE: use the original Intensity HDMI IN for authentic changing pixels and source-correlated timestamps from an unprotected known input, without confusing no-source callbacks or simulator data with real capture.

Outcome I — INDEPENDENCE: implement S/P/C with source-owned A124 driver, not vendor 10.4 kernel code, DeckLinkAPI, Black Syphon GUI or a firmware-updater service. Syphon is an optional downstream transport, not a device driver.

Outcome L — PORTABILITY: after the working A124 protocol is proven, port to Linux on a machine actually attached to that endpoint. This is an independent later deliverable, not a moving goalpost for S/P/C/I.

The user directs the work, not codes or operates the software. Agents own source, builds, permissions, OS configuration, troubleshooting and tests through legitimate authorized tools. Only an explicitly necessary physical plug change can be requested from the owner.

## 3. Proven starting evidence and current controls

OBSERVED: Macmini5,3, Sierra 10.12.6, original Thunderbolt 1/Comdis THUNDERDOCK2, A124 PCIe attached through the original Blackmagic 10.4 driver; two identified 1 MiB PCI BAR resources.

OBSERVED: Source HDMI OUT to Intensity HDMI IN, Intensity HDMI OUT to Panasonic HDMI IN, Thunderbolt unchanged. Mac output EDID currently identifies BMD HDMI at 1080p30. HDMI input receiver lock tests failed. Independent real HDMI input pixels have not been accepted.

OBSERVED: Original vendor APIs accepted sustained generated-frame output, including 5,400/5,400 frames over 90 seconds, and the hardware state changed from idle to playback busy. This did not establish a working Panasonic picture; the owner subsequently observed persistent on/off cycling.

OBSERVED: Intensity input connector bit 0x2, output connector mask 0xA (HDMI+component). Setting output mask to component-only returned nominal success but did not change readback; preferences commit and idle DisableVideoOutput returned access denied. Therefore physical HDMI OUT disable was NOT demonstrated.

OBSERVED: Two separate Fountain launch paths reactivated output after termination. They were disabled. A source-owned OUTPUT_DISABLED marker exists and the installed hardware playback executable is non-executable; guarded invocations fail before vendor calls. DO NOT remove these owner-requested stops during driver development.

OBSERVED: The Sierra passive IOKit PCI KEXT compiles and offline-link checks, but is unbound and untested as an active driver.

DOCUMENTED STATIC EVIDENCE: The original driver selects the HDExtreme model path for A124, model code 0x36. BAR0+0x40 is a candidate input status word, BAR0+0x224 a candidate firmware-version low byte. Their safety, physical values and complete meaning have not been established through independent real PCI access. Firmware, DMA descriptors, output PHY/transmitter control and IRQ completion remain unresolved.

OBSERVED: Recovery HD and exact original 10.4 installer are archived and verified, but the alternative Sierra boot/recovery pathway has not been tested. Ubuntu currently has NO locally attached A124 endpoint. Synthetic hardware interface tests are useful but do not count as a live driver.

References: emergency-stop report in tests/2026-10-25-panasonic-owner-emergency-stop.md; protocol and DMA evidence under docs/; Sierra recovery evidence under tests/2026-10-24-sierra-recovery-readiness-exact-rollback.md.

## 4. Engineering phases, dependencies and proof gates

### G0. Preserve owner-requested STOP and baseline

Read-only verify both launch paths are disabled, OUTPUT_DISABLED exists, original hardware output binary is non-executable, no Fountain output processes and the A124 reports idle. Record the owner's ongoing Panasonic on/off symptom as PHYSICAL STOP OPEN. Capture driver hashes, PCI and Thunderbolt topology, original output and input settings, device power status and current source timing.

Gate: accurate, reproducible machine-readable baseline and no unapproved playback. Do not claim the Panasonic is electrically silent.

### G1. Recover the model-specific A124 protocol

Use authorized private binary/firmware analysis and existing source-owned A124 dispatch evidence. Resolve the exact output transmitter enable/disable state, HDMI Hot Plug Detect and sink-status handling, clock and PHY power, firmware gates, BAR0/BAR1 ownership, buffer queues, DMA descriptor layout, IRQ acknowledgement and reset/stop semantics. Corroborate model-specific ABI and call sites independently; distinguish observed, static, inferred and unknown.

Deliverable: docs/a124-register-protocol.md with each register's address, width, endianness, read/write effects, origin, confidence, failure path and rollback. Unknown addresses or side effects remain UNKNOWN and cannot be tried live simply to gather data.

Gate: independently corroborated minimal output-off control contract for the first physical test; DMA protocol proof before starting physical frame DMA. Do not invent unrelated prerequisites.

### G2. Write the portable device-state and protocol core

Add driver/common containing explicit QUIESCED, DISCOVERED, OUTPUT_ARMED, OUTPUT_RUNNING, INPUT_ARMED, INPUT_RUNNING, FAULT and STOPPING transitions, typed register accesses, bounds checks, model/firmware validation, bounded DMA descriptor ownership, timestamps and cancellation. Keep the already working image kernel and media contracts as consumers.

Use only injected register values in simulation. Exercise failures for unknown offsets, incorrect firmware gate, wrong PCI ID, concurrent half-duplex requests, timing mismatch, interrupt storms, DMA-after-stop, memory overrun and timeout. No simulated test may claim electrical HDMI output.

Gate: deterministic local tests with source hashes on Sierra and Ubuntu, no original Blackmagic runtime required for the protocol model.

### G3. Implement and compile the Sierra PCI adapter OFFLINE

Extend driver/sierra/passive as the base. Make an IOKit PCI adapter for the typed core without any matching IOKit personality, automatic hardware attachment, BAR mapping, bus mastering or firmware writes on the normal boot. Validate ABI, SDK symbols, signing and dependency relationships using the actual Sierra compiler and current original kernel version.

Gate: signed/source-controlled or clearly labeled unsigned offline KEXT, build and symbol checks; prove it did not attach or replace the vendor driver. A successful offline compilation is NOT a functioning driver.

### G4. Prove exclusive physical-driver ownership can be recovered

The original driver currently owns A124. An active replacement must not compete with it. Before trying an active KEXT, establish an actually witnessed separate boot/test environment and recovery/rollback to the exact original 10.4 installation, with independent means to restore normal boot if SSH fails. Preserve all existing user/data volumes, especially Fresh. Alternative: if a compatible and isolated Ubuntu physical attachment is demonstrated first, the same exclusive-access/rollback gate can be satisfied on Ubuntu without imposing a second Sierra installation.

Gate: documented, repeatable test boot and rollback; only one A124 PCI owner; no firmware flash. This is binding for active takeover, NOT for coding or analysis.

### G5. First bounded independent real hardware reads

With exclusive PCI ownership and proven recovery, read only corroborated known registers in bounded observation, including hardware identity, model and firmware readiness. Establish safe BAR mapping and IRQ observation without starting HDMI capture or output. Compare the exact values against the original baseline and reconcile any differences before proceeding.

Gate: real hardware register values with timestamps, known register definitions, no memory corruption, clean release/rollback and independent witness. Static Ghidra offsets cannot pass this gate.

### G6. Implement electrical HDMI OUT quiet state — owner's FIRST physical result

Resolve the original A124 transmitter/PHY enable and its clock/HPD state. Implement source-owned transmitter-off and default unclaimed-off behavior with a no-auto-start guarantee; do not rely on the vendor configuration mask alone. Keep HDMI IN and Thunderbolt unchanged. If the hardware requires an explicit power/reset sequence, implement only a documented, recoverable sequence.

Gate S: an independent receiver or the actual Panasonic confirms that on/off cycling STOPS and stays stopped with the cable still attached. A clean Stop remains effective across service termination and restart. Do not claim success using busy=0, a process list, nominal SetInt success or synthetic signal state.

Current physical symptom must remain explicitly OPEN until this observation exists. Do not relaunch the retired Fountain playback services to test it.

### G7. Genuine controlled HDMI video output

Use the existing Fountain Die Maschine RGBA/UYVY generator. Implement physical output buffers, DMA completion, bounded video modes supported by original A124 (e.g. 720p60, 1080p25), output clock scheduling, hardware release and error handling. Start with bounded visible patterns, then a real moving artistic score. Support a deterministic software STOP that returns to G6.

Gate P: independently witnessed, stable, changing visual pixels on Panasonic without cycling, with correct picture cadence, plus reliable Stop. Accepted API submissions or a Mac GUI screenshot cannot count as Panasonic optical evidence.

### G8. Genuine HDMI input capture

Establish an exact, unprotected, supported source; use currently wired silver HDMI output where it passes source-format evidence. Implement receiver detection, firmware gate, input DMA, timestamps, no-source semantics and signal-loss recovery under exclusive driver ownership; obey real device half-duplex constraints. Do not repeat crashing original vendor frame-object paths.

Gate C: a verified sequence of changing, authentic non-synthetic pixels with meaningful timestamps from original A124 HDMI IN, clean loss/relock handling, no fabricated frame. An old Black Syphon Syphon server and a no-source callback are not acceptance.

### G9. Own the application/runtime integration

Expose independent video/audio capability and state to the already implemented native Fountain instrument, optional Syphon and Teatro/Reframe. Keep media provenance explicit. Validate Stop on crash, repeated mode changes, input/output exclusion, driver unload and recovery. Preserve available output and input connectors; analog and embedded audio may be staged separately if they are not required for the initial video objective.

Gate I: neither original Blackmagic KEXT nor DeckLinkAPI nor Black Syphon is required for accepted physical S/P/C on the isolated test host. The genuine creative interface can stop the HDMI connector reliably.

### G10. Release and optional real Linux port

Create source-owned reproducible local build, install, uninstall, signed/artifact hashes as appropriate, rollback and machine-readable acceptance receipts. No auto-playback on install or boot. A native Linux PCI adapter may now bind the same shared core once the actual A124 is physically present behind a working TB adapter/dock and correct PCI/IOMMU ownership is witnessed.

Gate: full S/P/C/I acceptance on original hardware, then separately full Linux physical proof if Linux deployment is desired. Synthetic Ubuntu replay can never substitute for real Linux A124 attachment. Git source is authoritative; no GitHub CI.

## 5. Explicit risk and authority constraints

- Never silently remove OUTPUT_DISABLED or restore either disabled launcher. A planning request is NOT an instruction to resume Panasonic playback.
- No undocumented register writes, DMA bus mastering or KEXT takeover on the normal headless Sierra boot.
- No driver upgrade or firmware updater as a substitute for an independent implementation.
- No hidden screen/GUI automation that attempts to bypass macOS authorization.
- No physical HDMI picture claims without a Panasonic-side or independent receiver witness; no false claim of electrical off based on idle state.
- Preserve original Sierra data, original driver and source-owned rollback paths; never repurpose or erase Fresh.
- Do not make relocation to Ubuntu, a new identity service, an invented security ceremony or a software simulator mandatory to fulfil the actual owner goal.
- Hardware may need a local physical cable/observer action eventually; that must be a single precisely scoped plug operation. All software work belongs to agents.

## 6. Reporting, Definition of Done and immediate work order

Each gate needs: source-owned commit; exact device/OS/driver version and source hash; PASS/FAIL/NOT TESTED; tests with real versus synthetic provenance; error and rollback proof. Previous failure reports remain immutable historical evidence. Do not move the DoD as each phase passes.

Immediate implementation queue, WITHOUT physical execution or re-enabling HDMI:
1. G0: confirm owner Stop controls and actual output state; record the Panasonic symptom as still OPEN.
2. G1: complete the HDExtreme HDMI transmitter-off and associated PHY/HPD register/firmware call chain from preserved vendor evidence.
3. G2: create a source-owned typed output-off state machine and negative tests against injected evidence.
4. G3: compile the corresponding passive Sierra PCI adapter offline, preserving the vendor KEXT.
5. G4: establish the verified exclusive-test/rollback path before any actual register write.
6. G5/G6: perform measured, rollback-safe real hardware control and independently witness Panasonic STOP.
7. G7/G8: prove physical video output and capture, then G9/G10 release.

**Finite success:** Panasonic stops switching with the cable still connected under independent software control; Fountain-owned video visibly plays and stops on command; original A124 yields real captured, timestamped pixels; the runtime uses an independent driver and cleanly restores the original machine. Ubuntu portability does not precede or replace these acceptances.

References:
- Intel Thunderbolt launch (2011): https://www.thunderbolttechnology.net/news/pr/thunderbolt%E2%84%A2-technology-fastest-data-connection-your-pc-just-arrived
- Intel PC update (2012): https://download.intel.com/newsroom/kits/computex/2012/pdfs/2012_Computex_Thunderbolt_FactSheet_English.pdf
- Linux Thunderbolt documentation: https://docs.kernel.org/admin-guide/thunderbolt.html
- FCIS founding conduct: https://github.com/Fountain-Coach/.github/blob/main/docs/FCIS-Non-Invention-and-Goal-Fidelity.md
- Independent hardware architecture: docs/independent-driver-decision.md
- Real Mac/Ubuntu attachment: docs/a124-hardware-fidelity-ubuntu.md
- Legacy passive driver: driver/sierra/passive/README.md
- Original recovery constraint: tests/2026-10-24-sierra-recovery-readiness-exact-rollback.md
- Owner stop record: tests/2026-10-25-panasonic-owner-emergency-stop.md
- Physical capture issue: https://github.com/Fountain-Coach/blackmagic-intensity-extreme-revival/issues/8
- Optional Ubuntu attachment: https://github.com/Fountain-Coach/blackmagic-intensity-extreme-revival/issues/9
