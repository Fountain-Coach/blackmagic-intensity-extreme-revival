# Sierra recovery readiness and exact original driver rollback — 24 October 2026

## Verdict

**Recovery files structurally valid; original driver rollback binaries recovered exactly; bootable system rollback NOT TESTED.** The original Intensity Extreme is not revived. Do not run the privileged Desktop Video 10.11.2 installer, flash card firmware, repartition any disk, or boot this headless Mac into Recovery unattended.

## Hardware and volumes (directly witnessed)

- Silver Intel Mac mini model Macmini5,3; active OS **Sierra 10.12.6** on internal HFS+ Data volume **disk0s2**, 405 GiB used.
- Internal **disk0s3**: 650 MB HFS+ Apple_Boot Recovery HD; mounted **read-only** solely for inspection, not booted.
- Internal **disk1s2 Fresh**: ~68 GiB of real existing media/projects; not a macOS installation; **do not erase, overwrite, repartition or select Fresh as installer destination**.
- No configured Time Machine destination, no witnessed bootable external rescue medium attached to the Sierra machine.
- Original Blackmagic Desktop Video 10.4 still loaded; Fountain Video native synthetic/browser Scratch unaffected; hardware capture safety guard still enabled.
- External NINJA2 NTFS and DockerDisk APFS visible on the modern Mac belong to other tasks and are NOT suitable disposable rescue media.

## Sierra Recovery HD verification (read-only)

    diskutil mount readOnly disk0s3

Actual mount table: /dev/disk0s3 on /Volumes/Recovery HD (hfs, local, read-only).

The recovery directory contains BaseSystem.dmg, BaseSystem.chunklist, boot.efi (x86_64 EFI), prelinkedkernel, PlatformSupport.plist, SystemVersion.plist and com.apple.Boot.plist. ProductVersion is 10.12.6 and supported model list explicitly includes Macmini5,3. Bless information contains the CoreServices boot folder and boot.efi designation.

A complete hdiutil verify reported BaseSystem.dmg CRC checksum **VALID**.

SHA-256 BaseSystem.dmg:
af3bb4ea49ffb330a66bbce511c530afc0df20e490387f08c42b62845a9f729d

SHA-256 boot.efi:
adcc7707da54dac2609b1a070a46541549f3b94aad93e80f51bc8240bf9f9155

The new repeatable, no-mutation witness passed on the real Sierra Mac:

    sh scripts/sierra-recovery-readonly-witness.sh

    stage=sierra_recovery_structural result=pass os=10.12.6 model=Macmini5,3 recovery_disk=disk0s3 readonly=1 image_crc=valid boot_efi_sha=matched image_sha=matched
    stage=original_vendor_driver result=pass version=10.4 kernel_sha=matched
    stage=recovery_boot result=NOT_TESTED remote_session_preserved=1
    stage=external_rescue result=NOT_WITNESSED backup_boot_tested=0
    stage=driver_replacement result=NO_GO firmware_update_authorized=0 physical_hdmi_accepted=0

The structural PASS does NOT prove that the Recovery HD actually boots or can recover failed third-party kernel extensions while the Mac is headless.

## Apple Sierra installer image preserved privately

Apple's current official older-OS instructions:
https://support.apple.com/en-euro/102662

Apple-hosted source object:
https://updates.cdn-apple.com/2019/cert/061-39476-20191023-48f365f4-0015-4c41-9f44-39d3d2aca067/InstallOS.dmg

The Apple Support link uses the HTTP-only updates-http hostname. The equivalent exact object path on Apple's TLS-capable updates.cdn-apple.com host returned HTTPS 200, the same declared object size and 2019 last-modified timestamp. TLS verification was NOT disabled.

    InstallOS_Sierra_10.12_Apple.dmg
    5,007,882,126 bytes
    SHA-256 c793c9aae9b59302b4b01a52aad387d7e4873cf00c48352afc1ffcc826cb0208

Private copy on Ubuntu (never committed to Git):
/home/benedikt/fountain-blackmagic-re/vendor-offline-2026-10-24/apple-recovery/InstallOS_Sierra_10.12_Apple.dmg

A second temporary copy was transferred to Sierra solely to check its integrity and embedded Apple installer package signature. No OS installer has been executed. This copy is to be removed after verification; the Ubuntu archive is canonical.

Apple media instructions:
https://support.apple.com/en-us/101578

## Recovery HD files archived separately (not a bootable disk)

A read-only copy of the recovery-files directory was streamed and checksum-verified on Ubuntu:

    Sierra-10.12.6-Recovery-HD-files.tar
    506,429,440 bytes
    SHA-256 2d608969ff56a760e95890c84e94b67df9f741c85984ca96da4104167861e2d8

Stored under the same private Ubuntu apple-recovery directory with 0600 permissions. Contains recovery boot.efi and BaseSystem.dmg. This is **not** a blessed bootable volume or full-System backup.

## EXACT Blackmagic Desktop Video 10.4 rollback installer

Blackmagic's own catalogue lists release "Desktop Video 10.4" (release ID 42bf66be6c274326b6f651f317a9b900). Its actual Download Only interaction returned an authentic signed sw.blackmagicdesign.com URL, downloaded on the modern Mac without personal registration.

    Blackmagic_Desktop_Video_Macintosh_10.4.zip
    size 101,451,025 bytes
    SHA-256 cc2143143bc836a6158360be968df7d20731da37ddbd7cd33fc7824dcef00ebb

ZIP integrity checked; it contains DesktopVideo_10.4.dmg, 101,670,828 bytes. DMG CRC verified. Actual "Install Desktop Video 10.4.pkg" passed Apple's pkgutil --check-signature:

- Developer ID Installer: Blackmagic Design Inc
- Trusted timestamp: 31 March 2015, 05:54:41 UTC
- Apple Developer ID chain.

The original signed installer driver metadata: version 10.4, SDK macosx10.9, explicit PCI match pcibdbd,a124.

**EXACT binary SHA-256 matches the two critical components installed on Sierra:**

- KEXT executable DeckLink_Driver:
  c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780
- Framework library DeckLinkAPI:
  6972a2b025cf333a6bbf2c8a1199af89a9f85c1ccfcb17d2055876251c558137

Thus this is not merely a compatible version label; the source-owned rollback installer contains the **same vendor driver and framework bytes** as the current Sierra setup. Not all plug-ins were compared, and returning from firmware changes remains unproven.

Official 10.4 ZIP archived mode 0600 privately on Ubuntu:

/home/benedikt/fountain-blackmagic-re/vendor-offline-2026-10-24/Blackmagic_Desktop_Video_Macintosh_10.4.zip

A supplementary snapshot of existing 10.4 driver/framework components was already preserved (SHA-256 d746c6e0e49fa8e95615be3d3960ae4cf6e8854219bdab7d3e714bf83271a80e). Neither package is a complete bootable OS image.

## Sierra-compatible replacement archived separately

Official 10.11.2 ZIP remains in Ubuntu private storage, source signature verified, exact PCI ID match and Sierra 10.12 support:
SHA-256 17c70ed05d1432aecc4ba48b49365d1ce0a551415923f36ecbfb2fa333515470

Original package/firmware hazard audit:
tests/2026-10-24-official-desktop-video-sierra-package-audit.md

Its installer removes/replaces KEXTs and DeckLinkAPI, installs updater-related services and a firmware updater launch agent, and requires restart. Signed installer and A124 match DO NOT prove physical HDMI frame acquisition or safe firmware behavior.

## Concrete gate to the isolated physical experiment

1. Provide a dedicated blank **32 GB installer USB** connected to the silver Mac. Building a bootable installer **ERASES that specifically selected USB**; never use Data, Fresh, NINJA2 or DockerDisk.
2. Provide a distinct known-blank external backup drive sized for the current ~405 GiB Data volume (at least ~500 GB, preferably 1 TB); establish a full restorable system backup or boot-tested Sierra clone. An installer USB alone is not rollback.
3. With local monitor, keyboard and Ethernet on Macmini5,3, **witness an actual rescue boot**, preserve original boot selection, and explicitly confirm subsequent normal Sierra/SSH boot.
4. Test genuine restoration from the backup, not just Recovery HD file validity, while leaving Fresh untouched.
5. Establish a **driver-only / no firmware mutation** boundary before trying the 10.11.2 vendor driver. A firmware updater may otherwise start when the OS loads.
6. Only then conduct bounded HDMI 1080p30 input lock, physical pixel/timestamp acquisition, crash-free shutdown and non-synthetic browser source acceptance.

**No unattended boot, driver installation, firmware update, destructive USB operation, or physical capture attempt was performed during this stage.**
