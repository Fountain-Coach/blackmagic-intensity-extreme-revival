#!/usr/bin/env python3
"""Read-only audit of an OFFLINE Blackmagic Desktop Video Mac installer.

Inputs are a downloaded vendor ZIP, plus two METADATA files extracted from
its package payload (the *DeckLink_Driver.kext*/Contents/Info.plist and the
DesktopVideo.pkg/PackageInfo). It NEVER runs the installer, executes a driver,
maps hardware, changes system settings or supplies an installer bypass.

The X.509 signature must be verified separately with Apple's
    pkgutil --check-signature /path/to/Install\\ Desktop\\ Video.pkg
and the extracted KEXT bundle with
    codesign --verify --verbose=2 /path/to/DeckLink_Driver.kext
This auditor reports what its inputs establish, not a physical capture PASS.

Example:
    python3 scripts/audit-offline-desktop-video.py \
      --zip /tmp/Blackmagic_Desktop_Video_Macintosh_10.11.2.zip \
      --zip-sha256 17c70ed05d1432aecc4ba48b49365d1ce0a551415923f36ecbfb2fa333515470 \
      --kext-info /tmp/fc-bmd-10112-DeckLink-Info.plist \
      --package-info /tmp/fc-bmd-10112-component/DesktopVideo.pkg/PackageInfo \
      --expected-version 10.11.2 --pci-id pcibdbd,a124
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import sys
import xml.etree.ElementTree as ET
import zipfile


def checksum(path: Path):
    h = hashlib.sha256()
    with path.open("rb") as fd:
        while block := fd.read(4 * 1024 * 1024):
            h.update(block)
    return h.hexdigest()


def inspect(zip_path, expected_sha, kext_info_path, pkg_info_path,
            expected_version, pci_id):
    errors = []
    actual_sha = checksum(zip_path)
    sha_match = actual_sha.lower() == expected_sha.lower()
    if not sha_match:
        errors.append("zip_sha256_mismatch")

    with zipfile.ZipFile(zip_path) as archive:
        corruption = archive.testzip()
        names = archive.namelist()
    expected_member = "Blackmagic_Desktop_Video_" + expected_version + ".dmg"
    archive_match = names == [expected_member] and corruption is None
    if not archive_match:
        errors.append("unexpected_zip_content")

    with kext_info_path.open("rb") as fd:
        kext = plistlib.load(fd)
    identity_match = (
        kext.get("CFBundleIdentifier") ==
        "com.blackmagic-design.desktopvideo.iokit.driver"
        and kext.get("CFBundleVersion") == expected_version
    )
    if not identity_match:
        errors.append("unexpected_driver_identity")

    entries = (kext.get("IOKitPersonalities", {})
                    .get("DeckLink_Driver", {})
                    .get("IONameMatch", []))
    device_match = pci_id in entries
    if not device_match:
        errors.append("device_not_listed")

    node = ET.parse(pkg_info_path).getroot()
    package_match = (
        node.attrib.get("version") == expected_version
        and node.attrib.get("identifier") ==
            "com.blackmagic-design.DesktopVideo"
        and node.attrib.get("auth") == "root"
    )
    if not package_match:
        errors.append("unexpected_installer_identity")
    scripts = node.find("scripts")
    script_names = {el.tag: el.attrib.get("file")
                    for el in scripts} if scripts is not None else {}
    root_install = node.attrib.get("auth") == "root"
    restart = node.attrib.get("postinstall-action") == "restart"
    destructive = bool(script_names.get("preinstall") and
                       script_names.get("postinstall"))

    return {
        "audit": "offline_desktop_video_sierra_candidate",
        "result": "pass" if not errors else "fail",
        "version": expected_version,
        "archive_sha256": actual_sha,
        "zip_checksum_verified": sha_match,
        "zip_entry_verified": archive_match,
        "driver_bundle_verified": identity_match,
        "driver_sdk": kext.get("DTSDKName", "unknown"),
        "device_pci_id": pci_id,
        "pci_match_verified": device_match,
        "pci_match_index": entries.index(pci_id) if device_match else None,
        "install_metadata_verified": package_match,
        "requires_root": root_install,
        "installer_requires_restart": restart,
        "preinstall_script_present": bool(script_names.get("preinstall")),
        "postinstall_script_present": bool(script_names.get("postinstall")),
        "destructive_installation_possible": destructive,
        "apple_signature_verified_by_this_script": False,
        "official_os_support_verified_by_this_script": False,
        "boot_recovery_verified_by_this_script": False,
        "physical_capture_verified": False,
        "driver_loaded": False,
        "errors": errors,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--zip", type=Path, required=True)
    p.add_argument("--zip-sha256", required=True)
    p.add_argument("--kext-info", type=Path, required=True)
    p.add_argument("--package-info", type=Path, required=True)
    p.add_argument("--expected-version", required=True)
    p.add_argument("--pci-id", required=True)
    args = p.parse_args()
    try:
        result = inspect(args.zip, args.zip_sha256, args.kext_info,
                         args.package_info, args.expected_version,
                         args.pci_id)
    except (OSError, ValueError, ET.ParseError, zipfile.BadZipFile) as exc:
        result = {"result": "fail", "errors": [type(exc).__name__],
                  "physical_capture_verified": False, "driver_loaded": False}
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["result"] == "pass" else 1


if __name__ == "__main__":
    sys.exit(main())
