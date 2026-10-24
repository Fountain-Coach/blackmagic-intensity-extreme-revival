# Original Intensity Extreme — official Sierra driver package audit

**Audited:** 24 October 2026. **Release selected for an eventual isolated trial:** Desktop Video 10.11.2, not 10.11.4. **No installer has been run.** Real HDMI input remains unaccepted.

## 1. Authoritative source and hardware identity

Blackmagic's live official catalogue is at:

    https://www.blackmagicdesign.com/api/support/us/downloads.json

The following exact records were retrieved from Blackmagic's own catalogue and the corresponding release notes on 24 October 2026.

| Release | Original date | Mac release identifier | Declared Mac OS support |
| --- | --- | --- | --- |
| Desktop Video 10.8.2 | 27 Oct 2016 | c7500a92ac594517a712bcd92e072264 | Sierra 10.12, El Capitan 10.11, Yosemite 10.10 |
| Desktop Video 10.9.11 | 15 Feb 2018 | 8f945a96e36940abbb52eaff19644eaf | Sierra 10.12 and High Sierra 10.13 |
| Desktop Video 10.11.2 | 6 Aug 2018 | 28c40e5147c34a3eacb279d74d576630 | Sierra 10.12 and High Sierra 10.13 |
| Desktop Video 10.11.4 | 23 Oct 2018 | ceb304a75100442e9e763d6b371690d0 | High Sierra 10.13 and Mojave 10.14; Sierra **not listed** |

Historical Blackmagic support release notes:

- https://www.blackmagicdesign.com/support/readme/28c40e5147c34a3eacb279d74d576630
- https://www.blackmagicdesign.com/support/readme/ceb304a75100442e9e763d6b371690d0

The reference target is an **original Thunderbolt Intensity Extreme**, PCI ID **0xBDBD:0xA124**, attached to the 2011 silver Intel Mac mini (Macmini5,3), macOS Sierra **10.12.6**, currently running Desktop Video **10.4**.

The catalogue is not sufficient proof of the individual hardware model. Therefore the true driver Info.plist from each **official signed installer package payload** was inspected directly.

## 2. Authenticated 10.11.2 archive

Blackmagic's actual website Download Only workflow returned a signed URL hosted by **sw.blackmagicdesign.com**. No identity or personal registration data was submitted. The temporary signed URL and signature tokens are private and **not in Git**.

The original downloaded archive:

    Blackmagic_Desktop_Video_Macintosh_10.11.2.zip
    size: 234380278 bytes
    SHA256: 17c70ed05d1432aecc4ba48b49365d1ce0a551415923f36ecbfb2fa333515470

The ZIP integrity check passed. It contains exactly:

    Blackmagic_Desktop_Video_10.11.2.dmg

The image passed **hdiutil verify** and was mounted READ ONLY on the modern Mac for offline inspection. The contained installer package is **Install Desktop Video 10.11.2.pkg**.

Apple **pkgutil --check-signature** identified:

- Developer ID Installer: **Blackmagic Design Inc (9ZGFBWLSYP)**;
- Trusted signing timestamp **31 July 2018 08:38:55 UTC**;
- Certificate chain to Apple Root CA.

The actual nested driver KEXT also independently passed:

    codesign --verify --verbose=2 <offline-extracted DeckLink_Driver.kext>

with "valid on disk", "satisfies its Designated Requirement", **Developer ID Application: Blackmagic Design Inc**, Team ID **9ZGFBWLSYP**.

The KEXT source was extracted **as data**, and the complete installer, scripts and firmware updater were NOT executed.

## 3. Exact A124 device-match and binary ABI

The actual signed 10.11.2 package's driver Info.plist reports:

    CFBundleIdentifier: com.blackmagic-design.desktopvideo.iokit.driver
    CFBundleVersion: 10.11.2
    DTSDKName: macosx10.11
    IOKitPersonalities/DeckLink_Driver/IONameMatch[27]: pcibdbd,a124
    Total PCI matching entries: 38

Driver kernel executable: **Mach-O 64-bit x86_64 KEXT**.

Its SHA-256 is:

    b59feb55dacbb92a99cd49b4958351d1ff832da85660af92ab0904755d9043b7

The 10.11.2 userspace DeckLinkAPI library is a signed-package-sourced **x86_64 and i386** universal binary. Its Mach-O load commands declare minimum macOS 10.7 and SDK 10.11. SHA-256:

    6298b7f0d9515945f2346bc6aa92728a7e4bd9a7d0061bc3f9dc6c9eeda87bcd

Thus **Sierra 10.12 + Intel x86_64 + exact A124 PCI match + authentic kernel/runtime files** is demonstrated at metadata/binary levels. This **does not** establish successful real input-frame delivery or the absence of the original 10.4 crash.

The verified 10.11.4 archive (SHA-256 f99c3232afa450272e4108750113ab2558434c98a49d93b21edab155857f004b) also contains A124 in its driver Info.plist, but official notes do NOT list Sierra 10.12; **it is not approved for this installation**.

## 4. Installer is DESTRUCTIVE, privileged and rebooting

Offline inspection of the 10.11.2 subpackage PackageInfo:

    identifier: com.blackmagic-design.DesktopVideo
    version: 10.11.2
    auth: root
    postinstall-action: restart
    preinstall: ./decklink-preflight
    postinstall: ./decklink-postflight
    payload: 366402 KB installed, 362 files (DesktopVideo subpackage only)

The source installer Distribution claims only generic OS X 10.8+ in its installation check; the more specific official release notes explicitly list Sierra. These are different scopes of evidence.

Read-only extraction of the exact signed package's installer shell scripts confirmed:

- **PREINSTALL deletes the active driver files** from /Library/Extensions, including DeckLink_Driver.kext and other Blackmagic kernel extensions; it also removes the previous DeckLinkAPI.framework, plug-ins, Blackmagic launch agents and launch daemons, and application-related configuration.
- **POSTINSTALL moves replacement signed KEXT bundles** from the installer staging directory to /Library/Extensions, rebuilds the working package layout, touches the kernel extensions directory, installs CoreMediaIO and audio plug-ins, and changes a login hook for DeckLink.
- The postinstall driver-authorization/driver-load branch is explicitly conditioned on a macOS version **greater than 10.12**; it is not evidence it will execute under Sierra. Other scripts and restart are still required.
- The package includes an actual **Desktop Video Firmware Updater.app** with many firmware resources and a LaunchAgent com.blackmagic-design.DesktopVideoFirmwareUpdater configured to restart the updater after unsuccessful exits. It also includes a DesktopVideoUpdater agent. This **could introduce a firmware-update process**, but we have not proven it flashes this specific A124 card. Firmware writing is NOT authorized by an ordinary driver compatibility audit.
- Restart is explicitly required. This installation is not a simple overwrite-free user-space update and cannot be undone merely by quitting an application.

Therefore **no installation, launchctl action, kext load, firmware update or reboot** was authorized or performed on the silver Mac mini.

## 5. Reproducible offline fail-closed verification

A source-owned Python standard library auditor checks the official ZIP checksum and ZIP entries, the original KEXT Info.plist, exact PCI ID, version and driver identifier, plus the installer PackageInfo.

    python3 scripts/audit-offline-desktop-video.py \
      --zip <trusted vendor ZIP path> \
      --zip-sha256 <recorded full SHA256> \
      --kext-info <extracted driver Info.plist> \
      --package-info <extracted DesktopVideo PackageInfo> \
      --expected-version 10.11.2 --pci-id pcibdbd,a124

Actual run on modern macOS Python 3.14: **PASS** for official ZIP and embedded metadata. A deliberate wrong SHA-256 yielded **FAIL with zip_sha256_mismatch**. This audit intentionally does NOT assert code signature, Sierra vendor support, boot recovery or physical capture; those must be witnessed separately, as above.

No GitHub CI. No redistribution of signed or proprietary installer binaries.

## 6. Private archival and limited original-driver baseline

A private file store on the Ubuntu Mac mini, external to Git and the public Estate:

    /home/benedikt/fountain-blackmagic-re/vendor-offline-2026-10-24/

Its directory permissions are **0700**, official ZIP **0600**. After transfer, the stored 10.11.2 ZIP's SHA-256 was independently recomputed on Ubuntu and **matched the official downloaded archive**.

The original Sierra 10.4 kernel-extension and DeckLink framework files were **read, never stopped or modified**, and streamed as a compressed archive to the same private directory:

    Sierra-active-Desktop-Video-10.4-components.tar.gz
    SHA256: d746c6e0e49fa8e95615be3d3960ae4cf6e8854219bdab7d3e714bf83271a80e

This limited archive includes original vendor kernel bundles and DeckLinkAPI.framework. **It is not a full 10.4 installer, bootable backup or tested system rollback**. It does not clear the recovery gate.

## 7. Live recovery inventory on Sierra

Actual live system:

    disk0s2: Apple_HFS Data, Sierra 10.12.6 (boot volume)
    disk0s3: Apple_Boot Recovery HD, 650 MB
    disk1s2: Apple_HFS Fresh, ~500GB, ~68GB populated

The second volume **Fresh** has no boot.efi or SystemVersion.plist; it is NOT a complete bootable Sierra installation. It must not be erased or overwritten.

There is **no configured Time Machine destination**, no confirmed bootable external recovery disk, and no demonstrated recovery boot and rollback of the original KEXTs. A Recovery HD partition exists, but has NOT been boot-tested as an independently recoverable fallback. There are about 60GB free on the Sierra boot volume.

### Mandatory go/no-go gate

**No-go** to running the signed 10.11.2 package on the working Sierra boot until ALL hold:

1. A separate **boot-tested** Sierra-capable recovery environment or equivalent independently witnessed full-system rollback exists, with original application files and kernel drivers preserved.
2. No automated or unintended Blackmagic firmware flashing can occur during a driver-only acceptance experiment; updater invocation must be understood or disabled safely in an isolated setting.
3. The exact A124 physical acceptance procedure has a bounded duration, reproducible 1080p30 source, real no-source/lock/bytes/timestamps logging, and deterministic device-release cleanup.
4. The currently running synthetic Fountain Video/Browser Scratch lane can recover independently even when the physical input fails.
5. User explicitly authorizes the high-impact driver replacement after reviewing the rollback and firmware boundaries.

Next action after the above: test 10.11.2 on the isolated recoverable Sierra environment, and accept the physical Intensity Extreme ONLY after stable input lock + changing real pixels + timestamps + crash-free stream shutdown. A signed installer and PCI match are **necessary, not sufficient**, evidence.

**Current conclusion:** Official Sierra/A124 10.11.2 installer acquired and independently audited; private signed file staged; current Sierra 10.4 system not modified; physical device revival remains OPEN.
