# Nikon D5200 HDMI to Intensity Extreme — camera profile

Date: 24 October 2026. Confirmed connection supplied by owner: **Nikon D5200 HDMI OUT → Blackmagic Intensity Extreme HDMI IN** on the Intel Sierra Mac mini. No MacBook HDMI source is requested or required.

## Camera controls to enable HDMI live view

The following are **Nikon D5200-specific**, not a generic menu copied from newer Nikon cameras.

| D5200 control | Start value | Reason |
| --- | --- | --- |
| Setup (wrench) → Video mode | **PAL** | Makes 25p, 50p and 50i movie formats available |
| Shooting → Movie settings → Frame size/frame rate | **1280 × 720 / 50p** | Progressive PAL starting format supported by Intensity Extreme |
| Setup → HDMI → Output resolution | **Auto** | Nikon's recommended HDMI negotiation setting |
| Setup → HDMI → Device control | **Off** | Disables HDMI-CEC remote control; Nikon recommends Off before Live View with CEC-capable devices |
| Setup → Flicker reduction | **50 Hz** | Optional for mains-light flicker; not a digital HDMI signal-lock requirement |
| Custom settings → c2 Auto off timers | **Long** | D5200 Live View timeout of ~20 minutes, not unlimited |
| Camera's physical **Lv** lever | **Live View ON** | Needed for moving video from lens, without starting movie recording |
| Rear **R** button | **Hide indicators** | Cycles live-view overlays; not a separate "HDMI on-screen display" menu on this model |

At 720p50 (and at 1080i50), the Nikon rear LCD **may turn off on connection to an HDMI device**. The D5200 reference manual explicitly describes this; do not conclude from the camera LCD alone that HDMI stopped working. This camera has a **Type C mini-HDMI** socket.

**Don't substitute menu options from newer Nikon bodies.** D5200's HDMI setup offers *Output resolution* and *Device control*; newer Nikon advice about HDMI "output display size 100%" or "Live view on-screen display Off" is not a D5200 menu path.

## Sources

- [Nikon D5200 reference manual, HDMI options p.147](https://www.manualshelf.com/manual/nikon/d5200/reference-manual-complete-instructions-english/page-164.html)
- [Nikon D5200 reference manual, Movie settings p.103](https://www.manualslib.com/manual/471723/Nikon-D5200.html?page=120)
- [Nikon D5200 reference manual, Live View/HDMI p.105](https://www.manualslib.com/manual/471723/Nikon-D5200.html?page=122)
- [Nikon D5200 auto-off timers p.160](https://www.manualslib.com/manual/471723/Nikon-D5200.html?page=177)
- [Nikon HDMI capture-camera models and recommended output](https://www.nikonimgsupport.com/na/NSG_article?articleNo=000053545&configured=1&lang=en_SG)
- [Blackmagic display mode enum (HD1080i50, HD1080i5994, HD720p5994)](https://chromium.googlesource.com/chromium/src/+/lkgr/third_party/decklink/mac/include/DeckLinkAPIModes.h)

## Fountain Video application adjustment

The Sierra native application now offers **Auto · Nikon D5200 (6 formats)**. It tests, in order:

1. PAL 720p50 (`hp50`)
2. PAL 1080i50 (`Hi50`)
3. PAL 1080p25 (`Hp25`)
4. NTSC 720p59.94 (`hp59`)
5. NTSC 1080i59.94 (`Hi59`)
6. NTSC 1080p29.97 (`Hp29`)

Manual exact 1080p30 and 720p60 remain available. Auto tries one mode at a time, preserving the card's half-duplex contract. This is a **sequential format scan**, not a claim that the hardware automatically announces the incoming mode.

The application can select modes and detect actual frames, but cannot alter the Nikon's camera menus through HDMI, change its physical Lv lever, or produce electrical HDMI input by itself. No real D5200 frame has yet been accepted; the connected camera's power and Live View states have not been remotely observed.

The software changes have compiled and passed converter/virtual-preview and live device-discovery self-tests on Sierra. The updated six-mode scan still requires physical-source acceptance.
