/*
 * Fountain Coach - Sierra public I/O Kit C++ ABI witness.
 * This is a compile-only derived type: it does not load a kernel extension,
 * open PCI hardware, map resources, or access registers.
 *
 * The first two ABI vtable entries are the offset-to-top and RTTI pointer.
 * Clang's index 291 for mapDeviceMemoryWithRegister therefore corresponds
 * to a runtime virtual dispatch at byte offset (291-2)*8 = 0x908.
 * Clang index 37 for getVirtualAddress corresponds to (37-2)*8 = 0x118.
 */
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOMemoryDescriptor.h>

class FCPCIVTableInspection : public IOPCIDevice {
public:
    virtual void fcVTableKey();
};

void FCPCIVTableInspection::fcVTableKey() {}

class FCMemoryMapVTableInspection : public IOMemoryMap {
public:
    virtual void fcVTableKey();
};

void FCMemoryMapVTableInspection::fcVTableKey() {}
