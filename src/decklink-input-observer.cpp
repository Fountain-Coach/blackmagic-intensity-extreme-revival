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
static int32_t onFrame(Observer* s, void* video, void*) {
    if (!video) return 0;
    ++s->frames;
    void** vt=*(void***)video;
    uint32_t flags=((GetFlags)vt[7])(video);
    if (flags & 0x80000000U) {
        ++s->noSource;
        return 0;
    }
    if (s->valid.fetch_add(1) != 0) return 0;
    const long width = ((GetDimension)vt[3])(video);
    const long height = ((GetDimension)vt[4])(video);
    const long rowBytes = ((GetDimension)vt[5])(video);
    if (width > 0 && width <= 8192 && height > 0 && height <= 8192 &&
        rowBytes > 0 && rowBytes <= 65536) {
        s->firstFrameWidth.store((uint32_t)width);
        s->firstFrameHeight.store((uint32_t)height);
        s->firstFrameRowBytes.store((uint32_t)rowBytes);
        void* bytes = nullptr;
        const int32_t brc = ((GetFrameBytes)vt[8])(video, &bytes);
        if (brc == 0 && bytes) {
            const size_t total = (size_t)rowBytes * (size_t)height;
            const size_t count = total < 4096 ? total : 4096;
            const uint8_t* p = (const uint8_t*)bytes;
            uint32_t hash = 2166136261u;
            for (size_t i=0; i<count; ++i) {
                hash ^= (uint32_t)p[i];
                hash *= 16777619u;
            }
            s->firstFrameSampleCount.store((uint32_t)count);
            s->firstFrameSampleHash.store(hash);
            s->firstFrameBytesOk.store(count > 0 ? 1u : 0u);
        }
    }
    int64_t time=0, duration=0;
    const int32_t trc = ((GetStreamTime)vt[11])(video, &time, &duration, 30000);
    if (trc == 0 && duration > 0) {
        s->firstFrameTime.store(time);
        s->firstFrameDuration.store(duration);
        s->firstFrameTimeOk.store(1u);
    }
    return 0;
}
static void* callbackTable[5]={
    (void*)queryObserver,(void*)addRef,(void*)releaseRef,
    (void*)onFormatChange,(void*)onFrame
};
int main(int argc, char** argv) {
    uint32_t requestedMode = 0;
    if (argc == 2 && (strcmp(argv[1],"--observe-1080p25") == 0 || strcmp(argv[1],"--observe-three-seconds") == 0)) requestedMode=0x48703235;
    else if (argc == 2 && strcmp(argv[1],"--observe-1080p30") == 0) requestedMode=0x48703330;
    else if (argc == 2 && strcmp(argv[1],"--observe-720p60") == 0) requestedMode=0x68703630;
    else { fprintf(stderr,"Usage: observer --observe-1080p25|--observe-1080p30|--observe-720p60\n"); return 64; }
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
    rc=((SetCallback)ivt[17])(input,&obs);
    printf("stage=set_callback status=0x%08x\n",(unsigned)rc);fflush(stdout);
    if(rc){exitCode=25;goto cleanup;}
    callbackInstalled=true;
    rc=((Enable)ivt[6])(input,requestedMode,0x32767579,0);
    printf("stage=enable_input status=0x%08x\n",(unsigned)rc);fflush(stdout);
    if(rc){exitCode=26;goto cleanup;}
    enabled=true;
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
    sleep(3);
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
    printf("stage=complete exit=%d\n",exitCode);
    return exitCode;
}
