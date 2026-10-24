// Fixed silver HDMI input: bounded no-pixel callback differential.
// Source is ONLY silver Macmini5,3 HDMI OUT -> original Intensity HDMI IN.
// Original Intensity HDMI OUT -> added reference monitor; same Sierra 10.12
// Mac mini owns Thunderbolt 1 original Desktop Video 10.4.
//
// Test whether original InputStatus 'visl' false is a false negative:
// attach a callback, StartStreams for 1.5 seconds even if visl false.
// Callback touches only proven safe GetFlags, NEVER GetBytes/GetStreamTime.
// A valid-flag frame is useful information, NOT independently accepted pixels.
// No actual recording, no output playback, no driver/hardware changes.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
struct Monitor{
    void** vtable;
    std::atomic<uint32_t> references,callbacks,nonnull,valid,noSignal;
    Monitor():vtable(nullptr),references(1),callbacks(0),nonnull(0),valid(0),noSignal(0){}
};
static const uint8_t INPUT_ID[16]={0xaf,0x22,0x76,0x2b,0xdf,0xac,0x48,0x46,0xaa,0x79,0xfa,0x88,0x83,0x56,0x09,0x95};
static const uint8_t STATUS_ID[16]={0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
static const uint8_t CALLBACK_ID[16]={0xdd,0x04,0xe5,0xec,0x74,0x15,0x42,0xab,0xae,0x4a,0xe8,0x0c,0x4d,0xfc,0x04,0x4a};
static const uint8_t UNKNOWN_ID[16]={0,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};
typedef void*(*Factory)();typedef int32_t(*Next)(void*,void**);
typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef uint32_t(*Release)(void*);
typedef int32_t(*Enable)(void*,uint32_t,uint32_t,uint32_t);
typedef int32_t(*NoArg)(void*);
typedef int32_t(*SetCallback)(void*,void*);
typedef int32_t(*GetFlag)(void*,uint32_t,bool*);
typedef uint32_t(*FrameFlags)(void*);
static void releaseObj(void*o){if(o)((Release)(*(void***)o)[2])(o);}
static int32_t query(void*o,const uint8_t raw[16],void**p){
 CFUUIDBytes id;memcpy(&id,raw,16);return ((QI)(*(void***)o)[0])(o,id,p);
}
static uint32_t add(Monitor* m){return ++m->references;}
static uint32_t drop(Monitor* m){return --m->references;}
static int32_t inquire(Monitor*m,CFUUIDBytes iid,void**out){
 if(!out)return (int32_t)0x80004003;
 if(!memcmp(&iid,CALLBACK_ID,16)||!memcmp(&iid,UNKNOWN_ID,16)){
     *out=m;add(m);return 0;
 }
 *out=nullptr;return (int32_t)0x80004002;
}
static int32_t format(Monitor*,uint32_t,void*,uint32_t){return 0;}
static int32_t arrived(Monitor*m,void*video,void*audio){
 (void)audio;
 ++m->callbacks;
 if(!video)return 0;
 ++m->nonnull;
 // Prior differential tests on the ORIGINAL driver showed GetFlags safe.
 const uint32_t flags=((FrameFlags)(*(void***)video)[7])(video);
 if(flags & 0x80000000u)++m->noSignal;
 else ++m->valid;
 return 0;
}
static void* callbackVtable[5]={
 (void*)inquire,(void*)add,(void*)drop,(void*)format,(void*)arrived
};
static bool sameSilver(){
 char model[64]={0};size_t n=sizeof(model);
 if(sysctlbyname("hw.model",model,&n,nullptr,0)||strcmp(model,"Macmini5,3"))return false;
 CGDirectDisplayID ids[32]={0};uint32_t count=0;
 if(CGGetOnlineDisplayList(32,ids,&count)!=kCGErrorSuccess)return false;
 unsigned hits=0;
 for(uint32_t k=0;k<count;++k){
     CGDirectDisplayID d=ids[k];
     if(CGDisplayVendorNumber(d)!=8961||CGDisplayPixelsWide(d)!=1920||
        CGDisplayPixelsHigh(d)!=1080||!CGDisplayIsOnline(d))continue;
     CGDisplayModeRef mode=CGDisplayCopyDisplayMode(d);
     if(mode){double hz=CGDisplayModeGetRefreshRate(mode);
         if(hz>29.9&&hz<30.1)++hits;
         CGDisplayModeRelease(mode);
     }
 }
 return hits==1;
}
int main(int argc,char**argv){
 if(argc!=2||strcmp(argv[1],"--observe-unlocked-frame-flags-1500ms"))return 64;
 if(!sameSilver())return 65;
 void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                 RTLD_NOW|RTLD_LOCAL);
 if(!lib)return 66;
 Factory f=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
 void* it=f?f():nullptr,*dev=nullptr,*in=nullptr,*status=nullptr;
 if(!it){dlclose(lib);return 67;}
 int32_t rc=((Next)(*(void***)it)[3])(it,&dev);releaseObj(it);
 if(rc||!dev){dlclose(lib);return 68;}
 rc=query(dev,INPUT_ID,&in);
 if(rc||!in){releaseObj(dev);dlclose(lib);return 69;}
 query(dev,STATUS_ID,&status);
 Monitor monitor;monitor.vtable=callbackVtable;
 void** iv=*(void***)in;
 bool enabled=false,callback=false,streams=false;
 rc=((Enable)iv[6])(in,0x48703330,0x32767579,0);
 printf("stage=bounded_input_flags enable_rc=0x%08x mode=1080p30\n",(unsigned)rc);
 if(!rc)enabled=true;
 if(enabled){
     bool locked=false;
     int32_t lrc=status?((GetFlag)(*(void***)status)[3])(
                        status,0x7669736c,&locked):(int32_t)0x80000001u;
     printf("stage=bounded_input_flags prestart_lock_rc=0x%08x locked=%s\n",
           (unsigned)lrc,lrc==0?(locked?"YES":"NO"):"UNKNOWN");
     rc=((SetCallback)iv[17])(in,&monitor);
     printf("stage=bounded_input_flags callback_rc=0x%08x\n",(unsigned)rc);
     if(!rc)callback=true;
 }
 if(callback){
     rc=((NoArg)iv[13])(in);
     printf("stage=bounded_input_flags start_streams_rc=0x%08x\n",(unsigned)rc);
     fflush(stdout);
     if(!rc)streams=true;
 }
 if(streams) {
     usleep(1500000);
     bool locked=false;
     int32_t lrc=status?((GetFlag)(*(void***)status)[3])(
                         status,0x7669736c,&locked):(int32_t)0x80000001u;
     printf("stage=bounded_input_flags poststream_lock_rc=0x%08x locked=%s\n",
            (unsigned)lrc,lrc==0?(locked?"YES":"NO"):"UNKNOWN");
     rc=((NoArg)iv[14])(in);
     printf("stage=bounded_input_flags stop_streams_rc=0x%08x\n",(unsigned)rc);
 }
 if(callback){
     rc=((SetCallback)iv[17])(in,nullptr);
     printf("stage=bounded_input_flags detach_callback_rc=0x%08x\n",(unsigned)rc);
 }
 if(enabled){
     rc=((NoArg)iv[7])(in);
     printf("stage=bounded_input_flags disable_input_rc=0x%08x\n",(unsigned)rc);
 }
 printf("stage=bounded_input_flags result=%s callbacks=%u nonnull=%u "
        "valid_flags=%u no_source_flags=%u "
        "pixel_bytes_read=0 vendor_GetStreamTime_called=0 "
        "recorded_frames=0\n",
        monitor.valid.load()>0?"VALID_FRAMES_FLAGGED":
        monitor.noSignal.load()>0?"ONLY_NO_SOURCE":"NO_FRAMES",
        monitor.callbacks.load(),monitor.nonnull.load(),
        monitor.valid.load(),monitor.noSignal.load());
 fflush(stdout);
 releaseObj(status);releaseObj(in);releaseObj(dev);dlclose(lib);
 return !streams?70:monitor.valid.load()>0?0:32;
}
