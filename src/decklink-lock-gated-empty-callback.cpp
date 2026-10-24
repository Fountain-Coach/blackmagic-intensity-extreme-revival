// Fixed self-loop: silver Mac HDMI OUT -> original Intensity Extreme HDMI IN.
// Lock-gated first-frame diagnostics; no KEXT, firmware, display or cabling changes.
// Fountain Coach — bounded input-stream observation (no media saved).
// Uses only an existing, locally installed DeckLinkAPI runtime.
// Copyright (c) 2026 Fountain Coach contributors. MIT.
#include <CoreFoundation/CoreFoundation.h>
#include <atomic>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
typedef void* (*Factory)();
typedef int32_t (*Query)(void*, CFUUIDBytes, void**);
typedef uint32_t (*Release)(void*);
typedef int32_t (*Next)(void*, void**);
typedef int32_t (*Enable)(void*, uint32_t, uint32_t, uint32_t);
typedef int32_t (*NoArg)(void*);
typedef int32_t (*SetCallback)(void*, void*);
typedef int32_t (*GetFlag)(void*, uint32_t, bool*);
typedef int32_t (*GetInt)(void*, uint32_t, int64_t*);
typedef int32_t (*GetCount)(void*, uint32_t*);
typedef uint32_t (*GetFlags)(void*);
typedef long (*GetDimension)(void*);
typedef int32_t (*GetFrameBytes)(void*, void**);
typedef int32_t (*GetStreamTime)(void*, int64_t*, int64_t*, int64_t);
static const unsigned char statusIID[16]={0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
static const unsigned char inputIID[16]={0xaf,0x22,0x76,0x2b,0xdf,0xac,0x48,0x46,0xaa,0x79,0xfa,0x88,0x83,0x56,0x09,0x95};
static const unsigned char callbackIID[16]={0xdd,0x04,0xe5,0xec,0x74,0x15,0x42,0xab,0xae,0x4a,0xe8,0x0c,0x4d,0xfc,0x04,0x4a};
static const unsigned char unknownIID[16]={0,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};

struct Observer {
    void** vtable;
    std::atomic<uint32_t> refs;
    std::atomic<uint32_t> frames;
    std::atomic<uint32_t> valid;
    std::atomic<uint32_t> noSource;
    std::atomic<uint32_t> formatChanges;
    /* First valid frame only; no frame buffer is retained. */
    std::atomic<uint32_t> firstFrameBytesOk, firstFrameWidth;
    std::atomic<uint32_t> firstFrameHeight, firstFrameRowBytes;
    std::atomic<uint32_t> firstFrameSampleCount, firstFrameSampleHash;
    std::atomic<uint32_t> firstFrameTimeOk;
    std::atomic<int64_t> firstFrameTime, firstFrameDuration;
};
static uint32_t addRef(Observer* s) {return ++s->refs;}
static uint32_t releaseRef(Observer* s) {return --s->refs;}
static int32_t queryObserver(Observer* s, CFUUIDBytes iid, void** out) {
    if (!out) return (int32_t)0x80004003;
    if (!memcmp(&iid,callbackIID,16) || !memcmp(&iid,unknownIID,16)) {
        *out = s; addRef(s); return 0;
    }
    *out = nullptr; return (int32_t)0x80004002;
}
static int32_t onFormatChange(Observer* s, uint32_t, void*, uint32_t) {
    ++s->formatChanges; return 0;
}
// Differential crash probe: deliberately never reads/dereferences the vendor frame.
static int32_t onFrame(Observer* s, void* video, void*) {
    (void)video;
    const uint32_t frame=++s->frames;
    if(frame==1) {
        printf("stage=callback_reached ordinal=1 vendor_frame_inspection=0\n");
        fflush(stdout);
    }
    return 0;
}
static void* callbackTable[5]={
    (void*)queryObserver,(void*)addRef,(void*)releaseRef,
    (void*)onFormatChange,(void*)onFrame
};
int main(int argc, char** argv) {
    uint32_t requestedMode = 0;
    if (argc == 2 && strcmp(argv[1],"--locked-1080p30-no-frame-read") == 0) requestedMode=0x48703330;
    else { fprintf(stderr,"Usage: locked-observer --locked-1080p30-no-frame-read\n"); return 64; }
    printf("stage=physical_rig mac_hdmi_out_to_original_blackmagic_hdmi_in=1 initial_pixel_format=2vuy\n");
    printf("stage=safety native_vendor_driver=unchanged physical_pci_ownership=Sierra host_sierra_reboot=0\n");
    printf("stage=requested_video_mode code=0x%08x\n",requestedMode);
    void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",RTLD_NOW|RTLD_LOCAL);
    if(!lib){fprintf(stderr,"stage=framework status=fail\n");return 20;}
    Factory factory=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    if(!factory){dlclose(lib);return 21;}
    void* iterator=factory();
    if(!iterator){dlclose(lib);return 22;}
    void** iterV=*(void***)iterator;
    void* device=nullptr;
    int32_t rc=((Next)iterV[3])(iterator,&device);
    ((Release)iterV[2])(iterator);
    if(rc || !device){dlclose(lib);return 23;}
    void** devV=*(void***)device;
    CFUUIDBytes uuid;memcpy(&uuid,inputIID,16);
    void* input=nullptr;
    rc=((Query)devV[0])(device,uuid,&input);
    printf("stage=input_interface status=0x%08x present=%d\n",(unsigned)rc,input!=nullptr);
    fflush(stdout);
    if(rc || !input){((Release)devV[2])(device);dlclose(lib);return 24;}
    void** ivt=*(void***)input;
    Observer obs;
    obs.vtable=callbackTable;
    obs.refs.store(1); obs.frames.store(0); obs.valid.store(0);
    obs.noSource.store(0); obs.formatChanges.store(0);
    obs.firstFrameBytesOk.store(0); obs.firstFrameWidth.store(0);
    obs.firstFrameHeight.store(0); obs.firstFrameRowBytes.store(0);
    obs.firstFrameSampleCount.store(0); obs.firstFrameSampleHash.store(0);
    obs.firstFrameTimeOk.store(0); obs.firstFrameTime.store(0);
    obs.firstFrameDuration.store(0);
    bool callbackInstalled=false, enabled=false, streams=false;
    int exitCode=0;
    rc=((Enable)ivt[6])(input,requestedMode,0x32767579,0);
    printf("stage=enable_input status=0x%08x\n",(unsigned)rc);fflush(stdout);
    if(rc){exitCode=26;goto cleanup;}
    enabled=true;
    {
        void* st=nullptr;
        CFUUIDBytes statusUUID; memcpy(&statusUUID,statusIID,16);
        const int32_t sres=((Query)devV[0])(device,statusUUID,&st);
        printf("stage=prestream_status rc=0x%08x present=%d\n",
               (unsigned)sres,st!=nullptr);fflush(stdout);
        if(sres || !st){exitCode=31;goto cleanup;}
        void** sv=*(void***)st;
        int consecutive=0;
        for (int i=0;i<12;++i) {
            bool locked=false;
            int32_t lrc=((GetFlag)sv[3])(st,0x7669736c,&locked);
            printf("stage=prestream_lock sample=%d rc=0x%08x locked=%s\n",
                   i,(unsigned)lrc,lrc==0?(locked?"yes":"no"):"unknown");
            fflush(stdout);
            if (lrc==0 && locked) ++consecutive; else consecutive=0;
            if(consecutive>=2) break;
            usleep(250000);
        }
        ((Release)sv[2])(st);
        if(consecutive<2){
            printf("stage=prestream_gate result=NO_GO stable_lock=0 start_streams=0\n");
            fflush(stdout);
            exitCode=32; goto cleanup;
        }
        printf("stage=prestream_gate result=PASS stable_lock=1\n");
        fflush(stdout);
    }
    rc=((SetCallback)ivt[17])(input,&obs);
    printf("stage=set_callback status=0x%08x\n",(unsigned)rc);fflush(stdout);
    if(rc){exitCode=25;goto cleanup;}
    callbackInstalled=true;
    rc=((NoArg)ivt[13])(input);
    printf("stage=start_streams status=0x%08x\n",(unsigned)rc);fflush(stdout);
    if(rc){exitCode=27;goto cleanup;}
    streams=true;
    {
        uint32_t queued=0;
        int32_t qrc=((GetCount)ivt[8])(input,&queued);
        printf("stage=queue_frames rc=0x%08x queued=%u\n",(unsigned)qrc,queued);
        void* st=nullptr; CFUUIDBytes statusUUID; memcpy(&statusUUID,statusIID,16);
        int32_t src=((Query)devV[0])(device,statusUUID,&st);
        printf("stage=status_interface rc=0x%08x present=%d\n",(unsigned)src,st!=nullptr);
        if(!src && st){
            auto svt=*(void***)st; bool locked=false;int64_t mode=0;
            int32_t lrc=((GetFlag)svt[3])(st,0x7669736c,&locked);
            int32_t mrc=((GetInt)svt[4])(st,0x6476696d,&mode);
            printf("stage=signal_lock rc=0x%08x locked=%d detected_mode_rc=0x%08x mode=0x%llx\n",(unsigned)lrc,locked,(unsigned)mrc,(unsigned long long)mode);
            ((Release)svt[2])(st);
        }
        fflush(stdout);
    }
    usleep(350000);
    {
        /* Post-observation lock check via the vendor status interface.
         * Never maps or reads PCI registers directly.
         */
        void* st = nullptr;
        CFUUIDBytes statusUUID;
        memcpy(&statusUUID, statusIID, 16);
        int32_t src = ((Query)devV[0])(device, statusUUID, &st);
        if (!src && st) {
            void** svt = *(void***)st;
            bool locked = false;
            int64_t detected = 0;
            int32_t lrc = ((GetFlag)svt[3])(st, 0x7669736c, &locked);
            int32_t mrc = ((GetInt)svt[4])(st, 0x6476696d, &detected);
            printf("stage=signal_lock_final rc=0x%08x locked=%d detected_mode_rc=0x%08x mode=0x%llx\n",
                   (unsigned)lrc, locked, (unsigned)mrc, (unsigned long long)detected);
            ((Release)svt[2])(st);
        } else {
            printf("stage=signal_lock_final rc=0x%08x locked=0 status_interface_present=0\n",
                   (unsigned)src);
        }
        fflush(stdout);
    }
cleanup:
    if(streams) {
        rc=((NoArg)ivt[14])(input);
        printf("stage=stop_streams status=0x%08x\n",(unsigned)rc);
        if(rc && !exitCode)exitCode=28;
    }
    if(callbackInstalled){
        rc=((SetCallback)ivt[17])(input,nullptr);
        printf("stage=remove_callback status=0x%08x\n",(unsigned)rc);
        if(rc && !exitCode)exitCode=29;
    }
    if(enabled){
        rc=((NoArg)ivt[7])(input);
        printf("stage=disable_input status=0x%08x\n",(unsigned)rc);
        if(rc && !exitCode)exitCode=30;
    }
    printf("stage=counts frames=%u valid=%u no_source=%u format_changes=%u observer_refcount=%u\n",
        obs.frames.load(),obs.valid.load(),obs.noSource.load(),obs.formatChanges.load(),obs.refs.load());
    printf("stage=first_valid_frame bytes_ok=%u width=%u height=%u row_bytes=%u sample_size=%u sample_fnv32=0x%08x timestamp_ok=%u time=%lld duration=%lld timescale=30000\n",
        obs.firstFrameBytesOk.load(), obs.firstFrameWidth.load(),
        obs.firstFrameHeight.load(), obs.firstFrameRowBytes.load(),
        obs.firstFrameSampleCount.load(), obs.firstFrameSampleHash.load(),
        obs.firstFrameTimeOk.load(), (long long)obs.firstFrameTime.load(),
        (long long)obs.firstFrameDuration.load());
    fflush(stdout);
    ((Release)ivt[2])(input);
    ((Release)devV[2])(device);
    dlclose(lib);
    printf("stage=complete exit=%d physical_capture_accepted=0 callback_differential_only=1\n",exitCode);
    return exitCode;
}
