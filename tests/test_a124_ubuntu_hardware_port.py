"""Ubuntu A124 hardware-fidelity interface tests: NO hardware I/O.

These unit tests use a private temporary sysfs fixture and never touch the
actual /sys PCI BARs, drivers, firmware, Thunderbolt authorization or DMA.
"""
from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from hardware.ubuntu.a124_pci_port import (
    A124VirtualRegisterPort, UnsupportedHardwareOperation, inventory,
    scan_pci, survey_thunderbolt, BAR_BYTES,
)


class HardwarePortTests(unittest.TestCase):
    def setUp(self):
        self._temp = tempfile.TemporaryDirectory(prefix="fc-a124-sysfs-")
        self.addCleanup(self._temp.cleanup)
        self.root = Path(self._temp.name)
        self.pci = self.root / "pci"
        self.pci.mkdir()
        self.tb = self.root / "tb"
        self.tb.mkdir()

    def attach_fake(self, vendor="0xbdbd", device="0xa124", sizes=None, flags="00000200"):
        node = self.pci / "0000:65:00.0"
        node.mkdir()
        (node / "vendor").write_text(vendor)
        (node / "device").write_text(device)
        lengths = sizes or [BAR_BYTES, BAR_BYTES]
        starts = [0xA1000000, 0xA2000000]
        lines = [f"{start:016x} {start + size - 1:016x} {flags}"
                 for start, size in zip(starts, lengths)]
        lines.extend(["0000000000000000 0000000000000000 00000000"] * 4)
        (node / "resource").write_text("\n".join(lines)+"\n")
        (self.root / "fake-driver").mkdir(exist_ok=True)
        (node / "driver").symlink_to(self.root / "fake-driver")
        (self.root / "42").mkdir(exist_ok=True)
        (node / "iommu_group").symlink_to(self.root / "42")
        return node

    def test_absent_endpoint_is_not_claimed_present(self):
        s=inventory(self.pci,self.tb)
        self.assertFalse(s["physical_intensity_extreme_present_on_this_host"])
        self.assertEqual(s["pci_functions"],[])
        self.assertIsNone(s["hdmi_signal_lock"])
        self.assertFalse(s["real_video_frame_received"])

    def test_pci_identity_and_exact_bar_extent(self):
        self.attach_fake()
        data=scan_pci(self.pci)
        self.assertEqual(len(data),1)
        self.assertEqual(data[0]["vendor"],"0xbdbd")
        self.assertEqual(data[0]["device"],"0xa124")
        self.assertEqual(data[0]["bdf"],"0000:65:00.0")
        self.assertEqual(data[0]["driver"],"fake-driver")
        self.assertEqual(data[0]["iommu_group"],"42")
        self.assertTrue(data[0]["bars_match_silver_witness"])
        self.assertEqual([v["size_bytes"] for v in data[0]["bars"]],
                         [BAR_BYTES, BAR_BYTES])
        self.assertFalse(data[0]["bars"][0]["bar_address_disclosed"])

    def test_wrong_pci_device_is_not_a124(self):
        self.attach_fake(device="0xa125")
        self.assertEqual(scan_pci(self.pci),[])

    def test_wrong_vendor_is_not_blackmagic(self):
        self.attach_fake(vendor="0x10b5")
        self.assertEqual(scan_pci(self.pci),[])

    def test_wrong_bar_geometry_is_not_promoted_to_witness(self):
        self.attach_fake(sizes=[BAR_BYTES, 16384])
        self.assertFalse(scan_pci(self.pci)[0]["bars_match_silver_witness"])

    def test_io_bar_is_not_memory_resource(self):
        self.attach_fake(flags="00000100")
        self.assertFalse(scan_pci(self.pci)[0]["bars_match_silver_witness"])

    def test_zeroed_resource_is_not_size_one(self):
        node=self.attach_fake()
        s=(node/"resource").read_text().splitlines()
        s[0]="0000000000000000 0000000000000000 00000000"
        (node/"resource").write_text("\n".join(s)+"\n")
        self.assertFalse(scan_pci(self.pci)[0]["bars"][0]["assigned"])
        self.assertEqual(scan_pci(self.pci)[0]["bars"][0]["size_bytes"],0)

    def test_invalid_sysfs_bar_line_fails_closed(self):
        node=self.attach_fake()
        (node/"resource").write_text("garbage\n0000000 0000000 0x200\n")
        with self.assertRaises(ValueError):
            scan_pci(self.pci)

    def test_host_only_thunderbolt_routers_are_not_external_cards(self):
        for name in ("domain0","domain1"):
            d=self.tb/name;d.mkdir()
            (d/"security").write_text("user\n")
            (d/"iommu_dma_protection").write_text("0\n")
        for name in ("0-0","1-0"):
            d=self.tb/name;d.mkdir()
            (d/"device_name").write_text("Macintosh\n")
        t=survey_thunderbolt(self.tb)
        self.assertEqual(len(t["domains"]),2)
        self.assertEqual(t["external_routers"],[])
        self.assertTrue(all(x["security"]=="user" for x in t["domains"]))
        self.assertFalse(t["iova_or_iommu_isolation_assumed"])

    def test_connected_but_unauthorized_is_not_pci_enumeration(self):
        d=self.tb/"0-1";d.mkdir()
        (d/"device_name").write_text("Intensity Extreme\n")
        (d/"authorized").write_text("0\n")
        t=survey_thunderbolt(self.tb)
        self.assertEqual(len(t["external_routers"]),1)
        self.assertEqual(t["external_routers"][0]["authorized"],"0")
        self.assertFalse(inventory(self.pci,self.tb)["physical_intensity_extreme_present_on_this_host"])

    def test_virtual_model_has_no_default_register_values(self):
        port=A124VirtualRegisterPort()
        self.assertEqual(port.provenance,"synthetic-register-replay")
        with self.assertRaises(UnsupportedHardwareOperation):
            port.read32(0,0x40)
        with self.assertRaises(UnsupportedHardwareOperation):
            port.read32(0,0x224)

    def test_only_evidence_backed_offsets_can_be_replayed(self):
        port=A124VirtualRegisterPort({0x224:0x0000000B,0x40:0x100})
        self.assertEqual(port.read32(0,0x224),0xB)
        self.assertEqual(port.read32(0,0x40),0x100)
        with self.assertRaises(UnsupportedHardwareOperation):
            port.read32(1,0x40)
        with self.assertRaises(UnsupportedHardwareOperation):
            port.read32(0,0x228)
        with self.assertRaises(UnsupportedHardwareOperation):
            A124VirtualRegisterPort({0x300:0})

    def test_invalid_register_width_value_rejected(self):
        with self.assertRaises(ValueError):
            A124VirtualRegisterPort({0x224:0x100000000})
        with self.assertRaises(ValueError):
            A124VirtualRegisterPort({0x224:-1})

    def test_no_mutating_virtual_port_operations(self):
        port=A124VirtualRegisterPort({0x224:0xB})
        for act in (
            lambda:port.write32(0,0x224,0xB),
            port.enable_dma,port.wait_for_interrupt,port.firmware_update,
        ):
            with self.assertRaises(UnsupportedHardwareOperation):
                act()

    def test_inventory_cannot_conflate_replay_with_hdmi_capture(self):
        self.attach_fake()
        rep=inventory(self.pci,self.tb)
        self.assertTrue(rep["physical_intensity_extreme_present_on_this_host"])
        self.assertFalse(rep["real_video_frame_received"])
        self.assertIsNone(rep["firmware_ready"])
        self.assertIsNone(rep["known_register_values_on_ubuntu"])
        self.assertFalse(rep["executed_dma"])
        self.assertFalse(rep["executed_device_writes"])
        self.assertFalse(rep["changed_kernel_driver"])
        self.assertIn('"hdmi_signal_lock": null',json.dumps(rep))


if __name__=="__main__":
    unittest.main()
