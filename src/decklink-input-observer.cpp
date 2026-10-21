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
typedef uint32_t (*GetFlags)(void*);
typedef long (*GetDimension)(void*);
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
    if (flags & 0x80000000U) ++s->noSource;
    else ++s->valid;
    return 0;
}
static void* callbackTable[5]={
    (void*)queryObserver,(void*)addRef,(void*)releaseRef,
    (void*)onFormatChange,(void*)onFrame
};
int main(int argc, char** argv) {
    if (argc != 2 || strcmp(argv[1],"--observe-three-seconds")) {
        fprintf(stderr,"usage: input-observer --observe-three-seconds\n");
        return 64;
    }
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
    bool callbackInstalled=false, enabled=false, streams=false;
    int exitCode=0;
    rc=((SetCallback)ivt[17])(input,&obs);
    printf("stage=set_callback status=0x%08x\n",(unsigned)rc);fflush(stdout);
    if(rc){exitCode=25;goto cleanup;}
    callbackInstalled=true;
    rc=((Enable)ivt[6])(input,0x48703235,0x32767579,0);
    printf("stage=enable_input status=0x%08x\n",(unsigned)rc);fflush(stdout);
    if(rc){exitCode=26;goto cleanup;}
    enabled=true;
    rc=((NoArg)ivt[13])(input);
    printf("stage=start_streams status=0x%08x\n",(unsigned)rc);fflush(stdout);
    if(rc){exitCode=27;goto cleanup;}
    streams=true;
    sleep(3);
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
    fflush(stdout);
    ((Release)ivt[2])(input);
    ((Release)devV[2])(device);
    dlclose(lib);
    printf("stage=complete exit=%d\n",exitCode);
    return exitCode;
}
