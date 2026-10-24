#!/bin/sh
# Sierra Recovery HD: READ-ONLY structural witness on the real Macmini5,3.
#
# Prerequisite: operator explicitly mounted disk0s3 read-only with
#   diskutil mount readOnly disk0s3
# This script NEVER mounts or unmounts, changes boot settings, starts
# installers, loads drivers, flashes firmware, restarts, or modifies disks.
# PASS means structural recovery files verified; NOT boot-tested recovery.
set -eu
vol="/Volumes/Recovery HD"
root="$vol/com.apple.recovery.boot"
expected_img="af3bb4ea49ffb330a66bbce511c530afc0df20e490387f08c42b62845a9f729d"
expected_boot="adcc7707da54dac2609b1a070a46541549f3b94aad93e80f51bc8240bf9f9155"
fail() { echo "stage=sierra_recovery result=fail reason=$1"; exit 30; }

test "$(uname -s)" = "Darwin" || fail not_macos
test "$(sw_vers -productVersion)" = "10.12.6" || fail unexpected_running_system
test "$(sysctl -n hw.model)" = "Macmini5,3" || fail unexpected_model
test "$(bless --getBoot)" = "/dev/disk0s2" || fail unexpected_startup_disk
mount | grep -F "/dev/disk0s3 on $vol (hfs, local, read-only)" >/dev/null ||
  fail recovery_not_mounted_readonly
for name in BaseSystem.dmg BaseSystem.chunklist boot.efi prelinkedkernel \
            PlatformSupport.plist SystemVersion.plist com.apple.Boot.plist; do
  test -f "$root/$name" || fail "missing_$name"
done
version=$(/usr/libexec/PlistBuddy -c "Print :ProductVersion" "$root/SystemVersion.plist")
test "$version" = "10.12.6" || fail recovery_version_mismatch
/usr/libexec/PlistBuddy -c "Print :SupportedModelProperties" \
  "$root/PlatformSupport.plist" | grep -F "Macmini5,3" >/dev/null ||
  fail missing_hardware_model
bless --info --mount "$vol" | grep -F "Blessed System File" >/dev/null ||
  fail missing_blession_information
sha_img=$(shasum -a 256 "$root/BaseSystem.dmg" | awk '{print $1}')
test "$sha_img" = "$expected_img" || fail recovery_image_changed
sha_boot=$(shasum -a 256 "$root/boot.efi" | awk '{print $1}')
test "$sha_boot" = "$expected_boot" || fail recovery_boot_efi_changed
hdiutil verify -quiet "$root/BaseSystem.dmg" >/dev/null ||
  fail recovery_dmg_checksum_invalid

driver="/Library/Extensions/DeckLink_Driver.kext/Contents/Info.plist"
test -f "$driver" || fail original_vendor_driver_missing
driver_version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleVersion" "$driver")
test "$driver_version" = "10.4" || fail unexpected_vendor_driver_version
kernel_bundle_hash=$(shasum -a 256 \
  /Library/Extensions/DeckLink_Driver.kext/Contents/MacOS/DeckLink_Driver | awk '{print $1}')
test "$kernel_bundle_hash" = "c4ec945aa46d07123788e66c219203026d1dcddffad20d1a55b8b5b4943d3780" ||
  fail original_vendor_kernel_changed

echo "stage=sierra_recovery_structural result=pass os=10.12.6 model=Macmini5,3 recovery_disk=disk0s3 readonly=1 image_crc=valid boot_efi_sha=matched image_sha=matched"
echo "stage=original_vendor_driver result=pass version=10.4 kernel_sha=matched"
echo "stage=recovery_boot result=NOT_TESTED remote_session_preserved=1"
echo "stage=external_rescue result=NOT_WITNESSED backup_boot_tested=0"
echo "stage=driver_replacement result=NO_GO firmware_update_authorized=0 physical_hdmi_accepted=0"
echo "stage=safety installer_runs=0 disk_writes=0 boot_config_changes=0 vendor_capture_calls=0"
