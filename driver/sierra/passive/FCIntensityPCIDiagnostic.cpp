/*
 * Fountain Coach — Intensity Extreme independent driver, passive skeleton.
 *
 * Source-owned IOKit driver for Sierra; NO firmware, bus mastering, MMIO,
 * DMA, IRQs or configuration writes. Default personality intentionally does
 * not match real PCI hardware. Do not install this over the vendor driver.
 *
 * Copyright (c) 2026 Fountain Coach contributors. MIT.
 */
#include <IOKit/IOService.h>
#include <IOKit/IODeviceMemory.h>
#include <IOKit/pci/IOPCIDevice.h>

class FCIntensityPCIDiagnostic : public IOService {
    OSDeclareDefaultStructors(FCIntensityPCIDiagnostic);

public:
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
};

OSDefineMetaClassAndStructors(FCIntensityPCIDiagnostic, IOService);

bool FCIntensityPCIDiagnostic::start(IOService *provider) {
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci)
        return false;

    /* PCI configuration space reads are safe to perform without ownership.
     * Do not enable memory access or bus mastering at this stage.
     */
    const UInt16 vendor = pci->configRead16((UInt8)kIOPCIConfigVendorID);
    const UInt16 device = pci->configRead16((UInt8)kIOPCIConfigDeviceID);
    if (vendor != 0xBDBD || device != 0xA124)
        return false;

    /* Each BAR was observed as 1 MiB by an independent live I/O Registry
     * probe. Obtain the provider-owned descriptors only; do not map, read
     * or write MMIO. Fail closed for an unexpected geometry.
     */
    IODeviceMemory *bar0 = pci->getDeviceMemoryWithRegister(
        kIOPCIConfigBaseAddress0);
    IODeviceMemory *bar1 = pci->getDeviceMemoryWithRegister(
        kIOPCIConfigBaseAddress1);
    if (!bar0 || !bar1)
        return false;
    const IOByteCount bytes0 = bar0->getLength();
    const IOByteCount bytes1 = bar1->getLength();
    if (bytes0 != 1024UL * 1024UL || bytes1 != 1024UL * 1024UL)
        return false;

    if (!IOService::start(provider))
        return false;

    setProperty("FCPCIReadOnly", true);
    setProperty("FCVendorID", (unsigned long long)vendor, 16);
    setProperty("FCDeviceID", (unsigned long long)device, 16);
    setProperty("FCBAR0Bytes", (unsigned long long)bytes0, 64);
    setProperty("FCBAR1Bytes", (unsigned long long)bytes1, 64);
    return true;
}

void FCIntensityPCIDiagnostic::stop(IOService *provider) {
    IOService::stop(provider);
}
