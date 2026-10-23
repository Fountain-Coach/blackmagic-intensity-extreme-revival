/*
 * Fountain Coach — Intensity Extreme PCI inventory (read-only).
 *
 * Uses macOS I/O Kit registry APIs, not DeckLinkAPI or Blackmagic software.
 * No kernel extensions, hardware writes, firmware changes, or DMA.
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

#define INTENSITY_VENDOR_ID 0xBDBDu
#define INTENSITY_DEVICE_ID 0xA124u

static bool read_u32_property(io_registry_entry_t entry, CFStringRef key, uint32_t *out) {
    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, kCFAllocatorDefault, 0);
    bool found = false;
    if (!value) {
        return false;
    }

    if (CFGetTypeID(value) == CFDataGetTypeID()) {
        CFDataRef data = (CFDataRef)value;
        if (CFDataGetLength(data) >= 4) {
            const UInt8 *b = CFDataGetBytePtr(data);
            /* IOPCIDevice's 4-byte PCI ID properties are little-endian. */
            *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
                   ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
            found = true;
        }
    } else if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        int64_t number = 0;
        if (CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &number)) {
            *out = (uint32_t)number;
            found = true;
        }
    }

    CFRelease(value);
    return found;
}

int main(void) {
    CFMutableDictionaryRef matching = IOServiceMatching("IOPCIDevice");
    if (!matching) {
        fprintf(stderr, "stage=matching result=failed\n");
        return 20;
    }

    io_iterator_t iterator = IO_OBJECT_NULL;
    kern_return_t kr = IOServiceGetMatchingServices(FC_IO_DEFAULT_PORT, matching, &iterator);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "stage=registry result=failed kr=0x%08x\n", (unsigned)kr);
        return 21;
    }

    unsigned scanned = 0;
    unsigned matched = 0;
    io_registry_entry_t entry;
    while ((entry = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        ++scanned;
        uint32_t vendor = 0, device = 0;
        bool have_vendor = read_u32_property(entry, CFSTR("vendor-id"), &vendor);
        bool have_device = read_u32_property(entry, CFSTR("device-id"), &device);
        if (have_vendor && have_device &&
            vendor == INTENSITY_VENDOR_ID && device == INTENSITY_DEVICE_ID) {
            ++matched;
            io_name_t io_class = {0};
            if (IOObjectGetClass(entry, io_class) != KERN_SUCCESS) {
                snprintf(io_class, sizeof(io_class), "unknown");
            }
            printf("stage=device result=found vendor=0x%04x device=0x%04x class=%s\n",
                   vendor, device, io_class);
        }
        IOObjectRelease(entry);
    }
    IOObjectRelease(iterator);

    printf("stage=inventory result=%s scanned=%u matched=%u\n",
           matched ? "found" : "not_found", scanned, matched);
    /* 2 means no matching hardware; it is not a driver or bus failure. */
    return matched ? 0 : 2;
}
