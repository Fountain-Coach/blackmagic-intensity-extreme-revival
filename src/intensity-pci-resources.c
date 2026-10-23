/*
 * Fountain Coach - Intensity Extreme PCI resource descriptors (read-only).
 *
 * Enumerates the IOKit PCI registry. Reports BAR descriptor offsets and
 * lengths, but deliberately never maps, reads or writes the BAR memory.
 * No DeckLinkAPI, driver takeover, firmware or kernel modification.
 *
 * Copyright (c) 2026 Fountain Coach contributors. MIT.
 */
#include <Availability.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 120000
#define FC_IO_DEFAULT_PORT kIOMainPortDefault
#else
#define FC_IO_DEFAULT_PORT kIOMasterPortDefault
#endif

static uint32_t le32(const UInt8 *p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool pci_property_u32(io_registry_entry_t e, CFStringRef key, uint32_t *out) {
    CFTypeRef x = IORegistryEntryCreateCFProperty(e, key, kCFAllocatorDefault, 0);
    bool valid = false;
    if (!x) return false;
    if (CFGetTypeID(x) == CFDataGetTypeID() &&
        CFDataGetLength((CFDataRef)x) >= 4) {
        *out = le32(CFDataGetBytePtr((CFDataRef)x));
        valid = true;
    }
    CFRelease(x);
    return valid;
}

static int describe_resources(io_registry_entry_t e) {
    CFTypeRef value = IORegistryEntryCreateCFProperty(
        e, CFSTR("assigned-addresses"), kCFAllocatorDefault, 0);
    if (!value) {
        printf("stage=resources result=missing\n");
        return 3;
    }
    if (CFGetTypeID(value) != CFDataGetTypeID()) {
        CFRelease(value);
        printf("stage=resources result=unexpected_type\n");
        return 4;
    }
    CFDataRef d = (CFDataRef)value;
    CFIndex bytes = CFDataGetLength(d);
    if (bytes <= 0 || bytes % 20 != 0 || bytes / 20 > 6) {
        printf("stage=resources result=unexpected_size bytes=%ld\n", (long)bytes);
        CFRelease(value);
        return 5;
    }
    const UInt8 *p = CFDataGetBytePtr(d);
    unsigned descriptors = (unsigned)(bytes / 20);
    printf("stage=resources descriptor_count=%u (registry_only)\n", descriptors);
    for (unsigned i = 0; i < descriptors; ++i) {
        const UInt8 *c = p + (size_t)i * 20;
        uint32_t space = le32(c);
        uint32_t size_hi = le32(c + 12);
        uint32_t size_lo = le32(c + 16);
        uint64_t size = ((uint64_t)size_hi << 32) | size_lo;
        uint32_t reg_offset = space & 0xFFU;
        printf("stage=pci_resource index=%u config_offset=0x%02x type=0x%02x size_bytes=%llu\n",
               i, reg_offset, (space >> 24) & 0xFFU,
               (unsigned long long)size);
    }
    CFRelease(value);
    return 0;
}

int main(void) {
    CFMutableDictionaryRef matching = IOServiceMatching("IOPCIDevice");
    if (!matching) return 20;
    io_iterator_t it = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(FC_IO_DEFAULT_PORT, matching, &it)
        != KERN_SUCCESS) return 21;
    io_registry_entry_t e;
    unsigned found = 0;
    int result = 2;
    while ((e = IOIteratorNext(it)) != IO_OBJECT_NULL) {
        uint32_t vendor = 0, device = 0;
        if (pci_property_u32(e, CFSTR("vendor-id"), &vendor) &&
            pci_property_u32(e, CFSTR("device-id"), &device) &&
            vendor == 0xBDBDU && device == 0xA124U) {
            ++found;
            printf("stage=target vendor=0x%04x device=0x%04x\n",vendor,device);
            result = describe_resources(e);
        }
        IOObjectRelease(e);
    }
    IOObjectRelease(it);
    printf("stage=complete matched=%u status=%d\n",found,result);
    return result;
}
