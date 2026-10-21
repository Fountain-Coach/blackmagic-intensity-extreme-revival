// Fountain Coach — read-only DeckLink API enumeration probe.
// Compiles without redistribution of Blackmagic SDK headers or frameworks.
// Default is read-only. --smoke-input requests then releases one capture mode.
// This tests the vendor API boundary; it does NOT replace the vendor driver.
// Copyright (c) 2026 Benedikt Eickhoff and Fountain Coach contributors. MIT.
#include <dlfcn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void* (*CreateIterator)(void);
typedef int32_t (*IteratorNext)(void*, void**);
typedef uint32_t (*ObjectRelease)(void*);
typedef int32_t (*GetModelName)(void*, CFStringRef*);
// On macOS, REFIID is CFUUIDBytes passed BY VALUE.
typedef int32_t (*QueryInterface)(void*, CFUUIDBytes, void**);
typedef int32_t (*GetModeIterator)(void*, void**);
typedef int32_t (*GetModeName)(void*, CFStringRef*);
typedef uint32_t (*GetModeCode)(void*);
typedef long (*GetDimension)(void*);
typedef int32_t (*GetFrameRate)(void*, int64_t*, int64_t*);
typedef int32_t (*GetAttributeInt)(void*, uint32_t, int64_t*);
typedef int32_t (*DoesSupportVideoMode)(void*, uint32_t, uint32_t, uint32_t, uint32_t*, void**);
typedef int32_t (*EnableVideoInput)(void*,uint32_t,uint32_t,uint32_t);
typedef int32_t (*DisableVideoInput)(void*);
static bool gSmokeInput = false;
static bool gSmokeOutput = false;

// API interface identifiers, not device registers or firmware bytes.
static const unsigned char kInputIID[16] = {0xAF,0x22,0x76,0x2B,0xDF,0xAC,0x48,0x46,0xAA,0x79,0xFA,0x88,0x83,0x56,0x09,0x95};
static const unsigned char kOutputIID[16] = {0xCC,0x5C,0x8A,0x6E,0x3F,0x2F,0x4B,0x3A,0x87,0xEA,0xFD,0x78,0xAF,0x30,0x05,0x64};
static const unsigned char kAttributesIID[16] = {0xAB,0xC1,0x18,0x43,0xD9,0x66,0x44,0xCB,0x96,0xE2,0xA1,0xCB,0x5D,0x31,0x35,0xC4};

static void probeAttributes(void* device) {
    QueryInterface query = (QueryInterface)(*(void***)device)[0];
    CFUUIDBytes uuid;
    memcpy(&uuid, kAttributesIID, sizeof(uuid));
    void* attributes = NULL;
    int32_t status = query(device, uuid, &attributes);
    printf("stage=query_attributes status=0x%08x present=%d\n", (unsigned)status, attributes != NULL);
    if (!attributes) return;
    void** table = *(void***)attributes;
    GetAttributeInt getInt = (GetAttributeInt)table[4];
    struct Key { const char* name; uint32_t id; };
    const Key keys[] = {
        {"video_io_support", 0x76696f73},
        {"video_input_connections", 0x7669636e},
        {"video_output_connections", 0x766f636e},
        {"maximum_audio_channels", 0x6d616368},
        {"device_busy_state", 0x64627374}
    };
    for (const Key& key : keys) {
        int64_t value = 0;
        status = getInt(attributes, key.id, &value);
        printf("stage=attribute name=%s status=0x%08x value=0x%llx\n", key.name, (unsigned)status, (unsigned long long)value);
    }
    ((ObjectRelease)table[2])(attributes);
}


static void probeInterface(void* device, const char* label, const unsigned char* iid) {
    void** deviceVtable = *(void***)device;
    QueryInterface query = (QueryInterface)deviceVtable[0];
    void* iface = NULL;
    CFUUIDBytes uuid;
    memcpy(&uuid, iid, sizeof(uuid));
    int32_t status = query(device, uuid, &iface);
    printf("stage=query_interface name=%s status=0x%08x present=%d\n", label, (unsigned int)status, iface != NULL);
    fflush(stdout);
    if (!iface) return;
    void** table = *(void***)iface;
    GetModeIterator getModes = (GetModeIterator)table[4];
    void* modes = NULL;
    status = getModes(iface, &modes);
    printf("stage=get_mode_iterator name=%s status=0x%08x present=%d\n", label, (unsigned int)status, modes != NULL);
    fflush(stdout);
    if (modes) {
        void** modeTable = *(void***)modes;
        IteratorNext next = (IteratorNext)modeTable[3];
        int modeCount = 0;
        for (int j=0; j<64; ++j) {
            void* mode = NULL;
            int32_t rc = next(modes, &mode);
            if (rc != 0 || !mode) break;
            modeCount++;
            void** entryTable = *(void***)mode;
            GetModeCode code = (GetModeCode)entryTable[4];
            GetDimension width = (GetDimension)entryTable[5];
            GetDimension height = (GetDimension)entryTable[6];
            GetFrameRate frameRate = (GetFrameRate)entryTable[7];
            GetModeName getName = (GetModeName)entryTable[3];
            int64_t duration = 0, scale = 0;
            int32_t rateStatus = frameRate(mode, &duration, &scale);
            CFStringRef name = NULL;
            char modeLabel[128] = {0};
            if (getName(mode, &name) == 0 && name) {
                CFStringGetCString(name, modeLabel, sizeof(modeLabel), kCFStringEncodingUTF8);
                CFRelease(name);
            }
            uint32_t currentMode = code(mode);
            printf("stage=mode interface=%s code=0x%08x width=%ld height=%ld rate=%lld/%lld rate_status=0x%08x name=%s\n", label, (unsigned)currentMode, width(mode), height(mode), (long long)scale, (long long)duration, (unsigned)rateStatus, modeLabel);
            if (currentMode == 0x48703235 || currentMode == 0x68703530) {
                DoesSupportVideoMode check = (DoesSupportVideoMode)table[3];
                const uint32_t pixels[] = {0x32767579, 0x76323130}; // 8-bit and 10-bit YUV
                for (uint32_t pixel : pixels) {
                    uint32_t supported = 0;
                    void* actualMode = NULL;
                    int32_t rc = check(iface, currentMode, pixel, 0, &supported, &actualMode);
                    printf("stage=pixel_support interface=%s mode=0x%08x pixel=0x%08x status=0x%08x support=%u\n", label, currentMode, pixel, (unsigned)rc, supported);
                    if (actualMode) ((ObjectRelease)(*(void***)actualMode)[2])(actualMode);
                }
            }
            ObjectRelease releaseMode = (ObjectRelease)(*(void***)mode)[2];
            releaseMode(mode);
        }
        printf("stage=modes name=%s count=%d\n", label, modeCount);
        ((ObjectRelease)modeTable[2])(modes);
    }
    if (gSmokeInput && strcmp(label, "input") == 0) {
        EnableVideoInput enable = (EnableVideoInput)table[6];
        DisableVideoInput disable = (DisableVideoInput)table[7];
        int32_t rc = enable(iface, 0x48703235, 0x32767579, 0); // 1080p25, 8-bit YUV
        printf("stage=enable_input mode=1080p25 pixel=8bit_yuv status=0x%08x\n", (unsigned)rc);
        fflush(stdout);
        if (rc == 0) {
            int32_t stop = disable(iface);
            printf("stage=disable_input status=0x%08x\n", (unsigned)stop);
        }
    }
    if (gSmokeOutput && strcmp(label, "output") == 0) {
        typedef int32_t (*EnableVideoOutput)(void*,uint32_t,uint32_t);
        typedef int32_t (*DisableVideoOutput)(void*);
        EnableVideoOutput enable = (EnableVideoOutput)table[6];
        DisableVideoOutput disable = (DisableVideoOutput)table[7];
        int32_t rc = enable(iface, 0x48703235, 0); // 1080p25
        printf("stage=enable_output mode=1080p25 status=0x%08x\n", (unsigned)rc);
        fflush(stdout);
        if (rc == 0) {
            int32_t stop = disable(iface);
            printf("stage=disable_output status=0x%08x\n", (unsigned)stop);
        }
    }
    ((ObjectRelease)table[2])(iface);
}


int main(int argc, char** argv) {
    gSmokeInput = argc == 2 && strcmp(argv[1], "--smoke-input") == 0;
    gSmokeOutput = argc == 2 && strcmp(argv[1], "--smoke-output") == 0;
    if (argc > 2 || (argc == 2 && !gSmokeInput && !gSmokeOutput)) { fprintf(stderr,"usage: decklink-smoke [--smoke-input|--smoke-output]\n"); return 64; }
    const char* library = "/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI";
    printf("probe: DeckLink API, %s\n", (gSmokeInput || gSmokeOutput) ? "bounded temporary video-mode activation" : "read-only device enumeration");
    fflush(stdout);
    void* handle = dlopen(library, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "stage=load_framework status=fail reason=%s\n", dlerror());
        return 20;
    }
    printf("stage=load_framework status=ok\n");
    CreateIterator create = (CreateIterator)dlsym(handle, "CreateDeckLinkIteratorInstance");
    if (!create) {
        fprintf(stderr, "stage=find_factory status=fail reason=%s\n", dlerror());
        dlclose(handle);
        return 21;
    }
    printf("stage=find_factory status=ok\n");
    fflush(stdout);
    void* iterator = create();
    if (!iterator) {
        printf("stage=create_iterator status=null devices=unknown\n");
        dlclose(handle);
        return 22;
    }
    printf("stage=create_iterator status=ok\n");
    fflush(stdout);
    void** vtable = *(void***)iterator;
    IteratorNext next = (IteratorNext)vtable[3];
    ObjectRelease release = (ObjectRelease)vtable[2];
    int count = 0;
    for (int i=0; i<16; ++i) {
        void* device = NULL;
        int32_t status = next(iterator, &device);
        printf("stage=next index=%d status=0x%08x present=%d\n", i, (unsigned int)status, device != NULL);
        fflush(stdout);
        if (device) {
            void** deviceVtable = *(void***)device;
            GetModelName getModel = (GetModelName)deviceVtable[3];
            CFStringRef model = NULL;
            int32_t result = getModel(device, &model);
            if (result == 0 && model) {
                char name[256] = {0};
                if (CFStringGetCString(model, name, sizeof(name), kCFStringEncodingUTF8))
                    printf("stage=device_model status=ok model=%s\n", name);
                else printf("stage=device_model status=conversion_error\n");
                CFRelease(model);
            } else printf("stage=device_model status=0x%08x\n", (unsigned int)result);
            probeAttributes(device);
            probeInterface(device, "input", kInputIID);
            probeInterface(device, "output", kOutputIID);
            ObjectRelease releaseDevice = (ObjectRelease)deviceVtable[2];
            releaseDevice(device);
            count++;
        }
        if (status != 0 || !device) break;
    }
    release(iterator);
    printf("stage=complete enumerated_devices=%d\n", count);
    dlclose(handle);
    return count > 0 ? 0 : 23;
}
