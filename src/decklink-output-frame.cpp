// Fountain Coach Intensity Extreme: generated, bounded single-frame video output test.
// No vendor headers or binaries are included in this source.
// Relies on the *installed* legacy DeckLinkAPI framework and driver.
// Copyright (c) 2026 Fountain Coach contributors. MIT.
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int32_t (*QueryInterface)(void*, CFUUIDBytes, void**);
typedef uint32_t (*Release)(void*);
typedef int32_t (*Next)(void*, void**);
typedef int32_t (*EnableOutput)(void*, uint32_t, uint32_t);
typedef int32_t (*DisableOutput)(void*);
typedef int32_t (*CreateFrame)(void*, int32_t, int32_t, int32_t, uint32_t, uint32_t, void**);
typedef int32_t (*GetBytes)(void*, void**);
typedef int32_t (*DisplaySync)(void*, void*);
typedef void* (*Factory)(void);

static const unsigned char outputIID[16] =
  {0xcc,0x5c,0x8a,0x6e,0x3f,0x2f,0x4b,0x3a,0x87,0xea,0xfd,0x78,0xaf,0x30,0x05,0x64};

int main(int argc, char** argv) {
    if (argc != 2 || strcmp(argv[1], "--frame-once") != 0) {
        fprintf(stderr, "Explicit opt-in: --frame-once (temporarily enables external video output)\n");
        return 64;
    }
    void* lib = dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI", RTLD_NOW|RTLD_LOCAL);
    if (!lib) { fprintf(stderr,"framework_load_error=%s\n",dlerror()); return 20; }
    Factory factory = (Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    if (!factory) { dlclose(lib); return 21; }
    void* it = factory();
    if (!it) { dlclose(lib); return 22; }
    void** itvt = *(void***)it;
    void* device = nullptr;
    int32_t status = ((Next)itvt[3])(it,&device);
    printf("stage=device status=0x%08x present=%d\n",(unsigned)status,device!=nullptr);
    fflush(stdout);
    ((Release)itvt[2])(it);
    if (status != 0 || !device) {dlclose(lib);return 23;}
    void** devt = *(void***)device;
    CFUUIDBytes iid; memcpy(&iid,outputIID,sizeof(iid));
    void* output = nullptr;
    status = ((QueryInterface)devt[0])(device,iid,&output);
    printf("stage=output_interface status=0x%08x present=%d\n",(unsigned)status,output!=nullptr);
    fflush(stdout);
    if (status != 0 || !output) {((Release)devt[2])(device);dlclose(lib);return 24;}
    void** ovt = *(void***)output;
    bool enabled = false;
    void* frame = nullptr;
    int exitCode = 0;
    // 1080p25 mode, cf. independently tested capability response.
    status = ((EnableOutput)ovt[6])(output,0x48703235,0);
    printf("stage=enable_output status=0x%08x\n",(unsigned)status);
    fflush(stdout);
    if (status != 0) {exitCode=25;goto cleanup;}
    enabled = true;
    status = ((CreateFrame)ovt[9])(output,1920,1080,3840,0x32767579,0,&frame);
    printf("stage=create_frame status=0x%08x present=%d\n",(unsigned)status,frame!=nullptr);
    fflush(stdout);
    if (status != 0 || !frame) {exitCode=26;goto cleanup;}
    {
        void** fvt = *(void***)frame;
        void* ptr = nullptr;
        status = ((GetBytes)fvt[8])(frame,&ptr);
        printf("stage=get_frame_bytes status=0x%08x present=%d\n",(unsigned)status,ptr!=nullptr);
        fflush(stdout);
        if (status != 0 || !ptr) {exitCode=27;goto cleanup;}
        unsigned char* b = (unsigned char*)ptr;
        const unsigned char bars[8][3] = {
            {128,235,128},{16,210,146},{166,170,16},{54,145,34},
            {202,106,222},{90,81,240},{240,41,110},{128,16,128}
        };
        for (int y=0; y<1080; ++y) {
            for (int pair=0; pair<960; ++pair) {
                int bar = (pair*2)/240; // 8 contiguous YUV color bars
                unsigned char* p=b + y*3840 + pair*4;
                p[0]=bars[bar][0]; p[1]=bars[bar][1];
                p[2]=bars[bar][2]; p[3]=bars[bar][1];
            }
        }
        printf("stage=fill_frame status=ok bytes=4147200 pattern=eight_yuv_bars\n");
        fflush(stdout);
        status = ((DisplaySync)ovt[11])(output,frame);
        printf("stage=display_frame_sync status=0x%08x\n",(unsigned)status);
        fflush(stdout);
        if (status != 0) exitCode=28;
    }
cleanup:
    if (frame) ((Release)(*(void***)frame)[2])(frame);
    if (enabled) {
        int32_t stop=((DisableOutput)ovt[7])(output);
        printf("stage=disable_output status=0x%08x\n",(unsigned)stop);
        if (stop != 0 && !exitCode) exitCode=29;
    }
    ((Release)ovt[2])(output);
    ((Release)devt[2])(device);
    dlclose(lib);
    printf("stage=complete exit=%d\n",exitCode);
    return exitCode;
}
