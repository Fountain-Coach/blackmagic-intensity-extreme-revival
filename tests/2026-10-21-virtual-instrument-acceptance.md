# First deployed virtual video-instrument acceptance — 21 October 2026

## Scope

This is an acceptance test of the **software simulator**. It is **not** a test of physical Blackmagic video input, firmware, electrical HDMI, analog connectors, or capture from a physical source. All frames in this test were deterministically synthesized in software.

- Code: [simulator/model.py](../simulator/model.py), [simulator/server.py](../simulator/server.py).
- Browser: [simulator/web/index.html](../simulator/web/index.html).
- Host: local Ubuntu Mac mini on a private WLAN (address deliberately omitted from public evidence).
- Python: 3.12.3 on Ubuntu; tested with only Python standard library.
- Live development deployment: dedicated LAN-only port 8172, bound to one local-network interface. Not a public Internet service.

## Regression evidence

```text
Ubuntu: Ran 22 tests in 0.788s
OK
```

This includes hardware-contract model tests and live HTTP tests on a temporary loopback server for health, capability inventory, frame-to-PNG preview and half-duplex rejection. Tests are invoked locally; **no GitHub CI** is required or configured.

## Network API acceptance

From a second machine on the same authorized WLAN:

1. `GET /health` returned `{"status":"ok","simulated":true,"physical_device_access":false}`.
2. `GET /api/state` returned a 720p50 virtual capture session, `signal_locked=true`, one generated frame, 1,843,200 bytes of YUV 4:2:2, CRC-32 `be2cc056`, and provenance `synthetic; not captured from physical HDMI`.
3. `GET /frame.png` returned a valid **320×180 RGB PNG** derived by decoding that virtual 1280×720 YUV frame. The preview is an eight-bar synthetic test pattern, not a static image downloaded from the Internet.
4. `POST /api/configure` selecting 1080p25 capture and a 720p50 source produced `signal_locked=false` and no delivered frame.
5. Advancing five virtual intervals produced `frames_delivered=0, frames_dropped=6` (one initial interval and five explicit steps).
6. `POST /api/half-duplex` while capture was active produced `blocked=true`, error `0x80000009`, without changing the active capture state.
7. Restoring 720p50 capture from a matching 720p50 source immediately returned `signal_locked=true` and one new virtual frame with explicit synthetic provenance.

## Separation of concerns

**Verified:** state-machine semantics, virtual signal-lock logic, half-duplex constraints, deterministic frame generation, presentation timestamps, frame checksums, generated image preview, HTTP transport on LAN, repeatable automated tests.

**Not verified by these tests:** HDMI receiver lock on Intensity Extreme, electrical capture, supported color signaling from real Macs, actual physical output to a monitor, analog input, audio samples, DMA, PCI or Thunderbolt driver independence, model accuracy for unknown firmware behaviors.

The virtual backend is a **development fixture** for a future typed Fountain Video Instrument contract. It must never be used to mark the physical Blackmagic's unresolved HDMI capture issue as resolved.

## Follow-on engineering

- Define a transport-agnostic typed frame/event schema with explicit capability origin and source provenance.
- Write an adapter enabling Reframe and Teatro to consume the simulator via a platform-neutral network endpoint (HTTP preview is the current demonstration, not a final streaming protocol).
- Add timestamped audio, fault replay, recording/replay assertions and backpressure simulations.
- Keep a second, separately testable native Blackmagic adapter built on the installed vendor driver; develop a true independent driver only with sufficient register/firmware evidence.
