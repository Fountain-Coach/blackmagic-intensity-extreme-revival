"""Read-only Linux PCIe attachment survey and fail-closed A124 replay interface.

Hardware identity is the ORIGINAL Thunderbolt Intensity Extreme PCI function:
    vendor 0xBDBD, device 0xA124; two observed 1 MiB BAR resources.

Physical Linux sysfs inventory:
    NEVER opens PCI BAR resource files, PCI config, /dev/mem or VFIO.
    NEVER binds/unbinds drivers, authorizes Thunderbolt, performs DMA or IRQ.

Virtual A124 port:
    Only models evidence-backed register *locations* at BAR0+0x40
    (input status), BAR0+0x224 (firmware word). The *values* must be
    explicitly injected as synthetic data; no authentic live register
    values have yet been witnessed. Unknown reads and ALL writes fail.

This is a userspace protocol/attachment model, NOT QEMU emulation of a
complete PCI endpoint, nor an independent physical capture driver.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
from typing import Any

VENDOR = 0xBDBD
DEVICE = 0xA124
BAR_BYTES = 1 << 20
# Evidence: see docs/hdextreme-protocol-obligations.md and
# tests/2026-10-23-pci-resource-layout.md.
KNOWN_READS = {0x40: "input_status", 0x224: "firmware_version_word"}
BDF = re.compile(r"^[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\.[0-7]$")
IORESOURCE_MEM = 0x200


class UnsupportedHardwareOperation(RuntimeError):
    """A request cannot be justified by the currently witnessed hardware."""


def _read_hex(path: Path) -> int:
    return int(path.read_text(encoding="ascii").strip(), 0)


def _read_label(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8").strip()
    except (OSError, UnicodeError):
        return None


def _bar(line: str, index: int) -> dict[str, Any]:
    """Linux PCI sysfs 'resource' entries: start end flags, inclusive."""
    try:
        start, end, flags = (int(x, 16) for x in line.split())
    except (ValueError, TypeError) as exc:
        raise ValueError(f"invalid PCI resource line {index}") from exc
    if start < 0 or end < 0 or flags < 0:
        raise ValueError(f"negative PCI resource line {index}")
    # A zeroed line signals an unassigned BAR, not a 1-byte resource.
    assigned = not (start == 0 and end == 0)
    length = end - start + 1 if assigned and end >= start else 0
    return {
        "bar": index,
        "assigned": assigned and end >= start,
        "size_bytes": length,
        "memory_resource": bool(flags & IORESOURCE_MEM),
        # No physical BAR addresses disclosed or required by this contract.
        "bar_address_disclosed": False,
        "flags": f"0x{flags:x}",
    }


def scan_pci(sysfs: Path = Path("/sys/bus/pci/devices")) -> list[dict[str, Any]]:
    """Only reads public Linux kernel metadata, not config or BAR memory."""
    found: list[dict[str, Any]] = []
    if not sysfs.is_dir():
        return found
    for entry in sorted(sysfs.iterdir()):
        if not entry.is_dir() or not BDF.fullmatch(entry.name):
            continue
        try:
            vendor = _read_hex(entry / "vendor")
            device = _read_hex(entry / "device")
        except (ValueError, OSError):
            continue
        if vendor != VENDOR or device != DEVICE:
            continue
        lines = (entry / "resource").read_text(encoding="ascii").splitlines()
        resources = [_bar(lines[n], n) for n in (0, 1)] if len(lines) >= 2 else []
        driver = (entry / "driver").resolve().name if (entry / "driver").is_symlink() else None
        group = (entry / "iommu_group").resolve().name if (entry / "iommu_group").is_symlink() else None
        exact_bars = (len(resources) == 2 and all(
            r["assigned"] and r["memory_resource"] and r["size_bytes"] == BAR_BYTES
            for r in resources
        ))
        found.append({
            "bdf": entry.name,
            "vendor": "0xbdbd",
            "device": "0xa124",
            "driver": driver,
            "iommu_group": group,
            "bars": resources,
            "bars_match_silver_witness": exact_bars,
            "physical_pci_enumerated": True,
            "registers_read": False,
            "dma_tested": False,
            "video_frames_accepted": False,
        })
    return found


def survey_thunderbolt(
    root: Path = Path("/sys/bus/thunderbolt/devices"),
) -> dict[str, Any]:
    """Separate TB domain policy from PCI device detection."""
    domains: list[dict[str, Any]] = []
    attached: list[dict[str, Any]] = []
    if root.is_dir():
        for entry in sorted(root.iterdir()):
            name = entry.name
            if name.startswith("domain") and name[6:].isdigit():
                domains.append({
                    "name": name,
                    "security": _read_label(entry / "security"),
                    "iommu_dma_protection": _read_label(entry / "iommu_dma_protection"),
                })
            # 0-0 and 1-0 are HOST routers, not attached equipment.
            elif re.fullmatch(r"[0-9]+-[0-9]+(?:\.[0-9]+)*", name):
                if name.endswith("-0"):
                    continue
                attached.append({
                    "path": name,
                    "device_name": _read_label(entry / "device_name"),
                    "vendor_name": _read_label(entry / "vendor_name"),
                    "authorized": _read_label(entry / "authorized"),
                })
    return {
        "domains": domains,
        "external_routers": attached,
        "device_auto_authorization_performed": False,
        "iova_or_iommu_isolation_assumed": False,
    }


class A124VirtualRegisterPort:
    """Injected read-only BAR0 register replay, not a physical PCI driver."""

    def __init__(self, values: dict[int, int] | None = None) -> None:
        self._values: dict[int, int] = {}
        for key, value in (values or {}).items():
            if key not in KNOWN_READS:
                raise UnsupportedHardwareOperation(f"unknown BAR0 register {key:#x}")
            if type(value) is not int or not 0 <= value <= 0xFFFFFFFF:
                raise ValueError(f"invalid synthetic register value: {key:#x}")
            self._values[key] = value

    @property
    def provenance(self) -> str:
        return "synthetic-register-replay"

    def read32(self, bar: int, offset: int) -> int:
        if bar != 0 or offset not in KNOWN_READS:
            raise UnsupportedHardwareOperation("unverified BAR/register offset")
        if offset not in self._values:
            raise UnsupportedHardwareOperation("no observed or injected value")
        return self._values[offset]

    def write32(self, bar: int, offset: int, value: int) -> None:
        raise UnsupportedHardwareOperation("all register writes forbidden in replay")

    def enable_dma(self) -> None:
        raise UnsupportedHardwareOperation("DMA protocol not yet established")

    def wait_for_interrupt(self) -> None:
        raise UnsupportedHardwareOperation("IRQ protocol not yet established")

    def firmware_update(self) -> None:
        raise UnsupportedHardwareOperation("firmware mutation forbidden")


def inventory(
    pci_root: Path = Path("/sys/bus/pci/devices"),
    thunderbolt_root: Path = Path("/sys/bus/thunderbolt/devices"),
) -> dict[str, Any]:
    devs = scan_pci(pci_root)
    return {
        "contract": "a124-hdextreme-pcie-v1",
        "execution_host": "ubuntu",
        "physical_intensity_extreme_present_on_this_host": bool(devs),
        "pci_functions": devs,
        "thunderbolt": survey_thunderbolt(thunderbolt_root),
        "silver_observed_bar_bytes": [BAR_BYTES, BAR_BYTES],
        "known_candidate_read_offsets_bar0": {
            "0x40": "input status",
            "0x224": "firmware version low byte",
        },
        "known_register_values_on_ubuntu": None,
        "firmware_ready": None,
        "hdmi_signal_lock": None,
        "real_video_frame_received": False,
        "external_observation_is_not_local_pci": True,
        "executed_pci_config_reads": False,
        "executed_pci_bar_reads": False,
        "executed_device_writes": False,
        "executed_dma": False,
        "authorized_thunderbolt_devices": False,
        "changed_kernel_driver": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pci-root", type=Path, default=Path("/sys/bus/pci/devices"))
    parser.add_argument("--thunderbolt-root", type=Path,
                        default=Path("/sys/bus/thunderbolt/devices"))
    parser.add_argument("--require-physical", action="store_true",
                        help="return 2 unless A124 is physically enumerated")
    args = parser.parse_args()
    result = inventory(args.pci_root, args.thunderbolt_root)
    print(json.dumps(result, indent=2, sort_keys=True))
    if args.require_physical and not result["physical_intensity_extreme_present_on_this_host"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
