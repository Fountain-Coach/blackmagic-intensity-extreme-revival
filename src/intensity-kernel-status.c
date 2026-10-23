/*
 * Fountain Coach - legacy Intensity Extreme driver attachment witness.
 * Pure I/O Registry reads, without opening the vendor user client.
 * A matching, powered driver is NOT proof of firmware readiness or HDMI lock.
 * Copyright (c) 2026 Fountain Coach contributors. MIT.
 */
#include <Availability.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 120000
#define FC_IO_DEFAULT_PORT kIOMainPortDefault
#else
#define FC_IO_DEFAULT_PORT kIOMasterPortDefault
#endif

static bool number(CFTypeRef v, uint64_t *n) {
    if (!v || CFGetTypeID(v) != CFNumberGetTypeID()) return false;
    int64_t i = 0;
    if (!CFNumberGetValue((CFNumberRef)v, kCFNumberSInt64Type, &i)) return false;
    *n = (uint64_t)i;
    return true;
}

static bool pci_id(io_registry_entry_t e, CFStringRef key, uint32_t *out) {
    CFTypeRef v=IORegistryEntryCreateCFProperty(e,key,kCFAllocatorDefault,0);
    bool ok=false;
    if (v && CFGetTypeID(v)==CFDataGetTypeID() &&
        CFDataGetLength((CFDataRef)v)>=4) {
        const UInt8 *b=CFDataGetBytePtr((CFDataRef)v);
        *out=(uint32_t)b[0]|((uint32_t)b[1]<<8)|
             ((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
        ok=true;
    } else if (v) {
        uint64_t n=0;
        if (number(v,&n)) { *out=(uint32_t)n; ok=true; }
    }
    if (v) CFRelease(v);
    return ok;
}

static bool registry_number(io_registry_entry_t e, CFStringRef key,
                            uint64_t *out) {
    CFTypeRef v=IORegistryEntryCreateCFProperty(e,key,kCFAllocatorDefault,0);
    bool ok=number(v,out);
    if (v) CFRelease(v);
    return ok;
}

static bool power_state(io_registry_entry_t e, CFStringRef name,
                         uint64_t *out) {
    CFTypeRef v=IORegistryEntryCreateCFProperty(
        e,CFSTR("IOPowerManagement"),kCFAllocatorDefault,0);
    bool ok=false;
    if (v && CFGetTypeID(v)==CFDictionaryGetTypeID()) {
        ok=number(CFDictionaryGetValue((CFDictionaryRef)v,name),out);
    }
    if (v) CFRelease(v);
    return ok;
}

static bool check_string(io_registry_entry_t e, CFStringRef key,
                          const char *expected) {
    CFTypeRef v=IORegistryEntryCreateCFProperty(e,key,kCFAllocatorDefault,0);
    char actual[128]={0};
    bool matched = v && CFGetTypeID(v)==CFStringGetTypeID() &&
        CFStringGetCString((CFStringRef)v, actual, sizeof(actual),
                           kCFStringEncodingUTF8) &&
        strcmp(actual,expected)==0;
    if (v) CFRelease(v);
    return matched;
}

int main(void) {
    CFMutableDictionaryRef match=IOServiceMatching(
        "com_blackmagic_design_iokit_DeckLink_Driver");
    if (!match) return 20;
    io_iterator_t it=IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(FC_IO_DEFAULT_PORT,match,&it)!=KERN_SUCCESS)
        return 21;
    unsigned services=0;
    io_registry_entry_t service;
    while ((service=IOIteratorNext(it))!=IO_OBJECT_NULL) {
        ++services;
        io_registry_entry_t parent=IO_OBJECT_NULL;
        kern_return_t parent_result=IORegistryEntryGetParentEntry(
            service,kIOServicePlane,&parent);
        uint32_t vendor=0,device=0;
        io_name_t parent_class={0};
        bool pci_parent=parent_result==KERN_SUCCESS &&
            IOObjectGetClass(parent,parent_class)==KERN_SUCCESS &&
            strcmp(parent_class,"IOPCIDevice")==0;
        bool ids=pci_parent &&
            pci_id(parent,CFSTR("vendor-id"),&vendor) &&
            pci_id(parent,CFSTR("device-id"),&device);
        printf("stage=vendor_service attached=1 parent_pci=%u vendor=0x%04x device=0x%04x\n",
               (unsigned)ids,vendor,device);
        uint64_t current=0,maximum=0,instance=0;
        bool c=power_state(service,CFSTR("CurrentPowerState"),&current);
        bool m=power_state(service,CFSTR("MaxPowerState"),&maximum);
        bool i=registry_number(service,CFSTR("DeckLinkInstanceNumber"),&instance);
        printf("stage=driver_published power_current_known=%u current=%llu power_max_known=%u max=%llu instance_known=%u instance=%llu\n",
               (unsigned)c,(unsigned long long)current,
               (unsigned)m,(unsigned long long)maximum,
               (unsigned)i,(unsigned long long)instance);
        unsigned audio=0,client=0,other=0;
        io_iterator_t children=IO_OBJECT_NULL;
        if (IORegistryEntryGetChildIterator(service,kIOServicePlane,&children)==KERN_SUCCESS) {
            io_registry_entry_t child;
            while ((child=IOIteratorNext(children))!=IO_OBJECT_NULL) {
                io_name_t class_name={0};
                if (IOObjectGetClass(child,class_name)==KERN_SUCCESS) {
                    if (strstr(class_name,"Audio")!=NULL) ++audio;
                    else if (strstr(class_name,"UserClient")!=NULL) ++client;
                    else ++other;
                }
                IOObjectRelease(child);
            }
            IOObjectRelease(children);
        }
        printf("stage=published_children audio=%u user_clients=%u other=%u\n",
               audio,client,other);
        bool name=check_string(service,CFSTR("IONameMatched"),"pcibdbd,a124");
        printf("stage=pci_match selected=%u\n",(unsigned)name);
        if (parent!=IO_OBJECT_NULL) IOObjectRelease(parent);
        IOObjectRelease(service);
    }
    IOObjectRelease(it);
    printf("stage=conclusion services=%u firmware_ready=unknown dma_ready=unknown receiver_locked=unknown\n",services);
    return services==1?0:2;
}
