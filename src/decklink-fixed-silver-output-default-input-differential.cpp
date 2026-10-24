// Bounded A/B/A diagnosis of original Intensity Extreme input lock versus
// volatile Blackmagic idle HDMI OUT standard. Fixed source, not a new fixture.
// A1: idle 1080i50 -> input lock; B: idle 1080p30 -> input lock;
// A2: restore idle 1080i50 -> input lock. Always black idle pixels.
// No StartStreams, playback, capture pixels, firmware, KEXT or preference writes.
// REQUIRE caller to quit Fountain Video and exclude other vendor clients first.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef void* (*Factory)();
typedef int32_t (*Next)(void*,void**);
typedef int32_t (*QI)(void*,CFUUIDBytes,void**);
typedef uint32_t (*Release)(void*);
typedef int32_t (*GetInt)(void*,uint32_t,int64_t*);
typedef int32_t (*SetInt)(void*,uint32_t,int64_t);
typedef int32_t (*GetFlag)(void*,uint32_t,bool*);
typedef int32_t (*EnableInput)(void*,uint32_t,uint32_t,uint32_t);
typedef int32_t (*NoArg)(void*);
static const uint8_t configIID[16]={
  0x1e,0x69,0xfc,0xf6,0x42,0x03,0x49,0x36,0x80,0x76,0x2a,0x9f,0x4c,0xfd,0x50,0xcb
};
static const uint8_t statusIID[16]={
  0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46
};
static const uint8_t inputIID[16]={
  0xaf,0x22,0x76,0x2b,0xdf,0xac,0x48,0x46,0xaa,0x79,0xfa,0x88,0x83,0x56,0x09,0x95
};
static const uint32_t HI50=0x48693530u, HP30=0x48703330u;
static const uint32_t BLACK=0x626c6163u, HDMI=0x2u, UYVY=0x32767579u;
static const uint32_t DVOM=0x64766f6du, VOIO=0x766f696fu;
static const uint32_t VICN=0x7669636eu, VISL=0x7669736cu;
static volatile sig_atomic_t interrupted=0;
static void signalHandler(int) { interrupted=1; }
static void release(void* o) { if(o)((Release)(*(void***)o)[2])(o); }
static int32_t query(void* object,const uint8_t bytes[16],void** out) {
  CFUUIDBytes iid;memcpy(&iid,bytes,16);
  return ((QI)(*(void***)object)[0])(object,iid,out);
}
static bool isFixedSilverSource() {
  char model[64]={0};size_t sz=sizeof(model);
  if(sysctlbyname("hw.model",model,&sz,0,0)||strcmp(model,"Macmini5,3"))return false;
  CGDirectDisplayID ds[32]={0};uint32_t n=0;
  if(CGGetOnlineDisplayList(32,ds,&n)!=kCGErrorSuccess)return false;
  unsigned matches=0;
  for(uint32_t i=0;i<n;++i) {
    CGDirectDisplayID d=ds[i];
    if(CGDisplayVendorNumber(d)!=8961||!CGDisplayIsActive(d)||
       CGDisplayPixelsWide(d)!=1920||CGDisplayPixelsHigh(d)!=1080)continue;
    CGDisplayModeRef m=CGDisplayCopyDisplayMode(d);
    if(m) {
      const double hz=CGDisplayModeGetRefreshRate(m);
      if(hz>29.9&&hz<30.1)++matches;
      CGDisplayModeRelease(m);
    }
  }
  return matches==1;
}
struct Sample { bool observed; bool locked; unsigned positive; int32_t enableRc; int32_t disableRc; };
static Sample probe(void* input,void* status,const char* phase) {
  Sample s={false,false,0,0,0};
  s.enableRc=((EnableInput)(*(void***)input)[6])(input,HP30,UYVY,0);
  if(s.enableRc!=0) {
    printf("stage=hdmi_idle_input_differential phase=%s enable_rc=0x%08x lock=NOT_TESTED\n",
           phase,(unsigned)s.enableRc);
    return s;
  }
  unsigned stable=0;
  for(unsigned j=0;j<12 && !interrupted;++j) {
    bool locked=false;
    int32_t rc=((GetFlag)(*(void***)status)[3])(status,VISL,&locked);
    if(rc==0) {
      s.observed=true;
      if(locked)++s.positive;
      stable=locked?stable+1:0;
      if(stable>=2)s.locked=true;
    } else stable=0;
    if(s.locked)break;
    if(j<11)usleep(250000);
  }
  s.disableRc=((NoArg)(*(void***)input)[7])(input);
  printf("stage=hdmi_idle_input_differential phase=%s input=Hp30 idle_standard=%s "
         "enable_rc=0x%08x status_observed=%u locked_samples=%u stable_lock=%u "
         "disable_rc=0x%08x callbacks=0 frames=0\n",
         phase,!strcmp(phase,"B")?"Hp30":"Hi50",(unsigned)s.enableRc,
         s.observed?1:0,s.positive,s.locked?1:0,(unsigned)s.disableRc);
  fflush(stdout);
  return s;
}
int main(int argc,char**argv) {
  if(argc!=2||strcmp(argv[1],"--compare-volatile-idle-standards"))return 64;
  if(!isFixedSilverSource()) { puts("stage=hdmi_idle_input_differential result=WRONG_FIXED_SOURCE"); return 20; }
  signal(SIGINT,signalHandler);signal(SIGTERM,signalHandler);
  void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",RTLD_NOW|RTLD_LOCAL);
  if(!lib)return 21;
  Factory f=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
  void* it=f?f():0,*dev=0,*cfg=0,*input=0,*status=0;
  if(!it) { dlclose(lib);return 22; }
  int32_t rc=((Next)(*(void***)it)[3])(it,&dev);release(it);
  if(rc||!dev) { dlclose(lib);return 23; }
  if(query(dev,configIID,&cfg)||!cfg||query(dev,inputIID,&input)||!input||
     query(dev,statusIID,&status)||!status) {
    release(status);release(input);release(cfg);release(dev);dlclose(lib);return 24;
  }
  const GetInt get=(GetInt)(*(void***)cfg)[6];
  const SetInt set=(SetInt)(*(void***)cfg)[5];
  int64_t original=0,idle=0,conn=0;
  int32_t mrc=get(cfg,DVOM,&original),irc=get(cfg,VOIO,&idle),crc=get(cfg,VICN,&conn);
  printf("stage=hdmi_idle_input_differential preflight mode_rc=0x%08x mode=0x%08x "
         "idle_rc=0x%08x idle=0x%08x input_rc=0x%08x input=0x%08x\n",
         (unsigned)mrc,(unsigned)(uint32_t)original,(unsigned)irc,
         (unsigned)(uint32_t)idle,(unsigned)crc,(unsigned)(uint32_t)conn);
  int result=0;
  if(mrc||irc||crc||(uint32_t)original!=HI50||
     (uint32_t)idle!=BLACK||(uint32_t)conn!=HDMI) {
    puts("stage=hdmi_idle_input_differential result=UNEXPECTED_DEVICE_STATE changed=0");
    result=25;
  } else {
    Sample a1=probe(input,status,"A1");
    if(a1.disableRc||a1.enableRc||!a1.observed||interrupted)result=26;
    bool setSucceeded=false,changed=false,restored=false;
    Sample b={false,false,0,0,0},a2={false,false,0,0,0};
    if(!result) {
      rc=set(cfg,DVOM,HP30);
      setSucceeded=rc==0;
      int64_t actual=0,stillBlack=0;
      int32_t r1=get(cfg,DVOM,&actual),r2=get(cfg,VOIO,&stillBlack);
      changed=setSucceeded&&r1==0&&r2==0&&(uint32_t)actual==HP30&&
              (uint32_t)stillBlack==BLACK;
      printf("stage=hdmi_idle_input_differential volatile_mode_set rc=0x%08x "
             "verified=%u read_mode_rc=0x%08x read_idle_rc=0x%08x "
             "preferences_written=0 idle_black=%u\n",
             (unsigned)rc,changed?1:0,(unsigned)r1,(unsigned)r2,
             r2==0&&(uint32_t)stillBlack==BLACK?1:0);
      if(changed&&!interrupted)b=probe(input,status,"B");
      else result=27;
    }
    // Even an unsuccessful readback MUST NOT skip rollback after SetInt.
    // Never write vendor preferences and never touch any other config key.
    if(setSucceeded) {
      int64_t actual=0,stillBlack=0;
      int32_t checkModeRc=get(cfg,DVOM,&actual);
      int32_t checkIdleRc=get(cfg,VOIO,&stillBlack);
      if(checkModeRc==0&&checkIdleRc==0&&(uint32_t)actual==HP30&&
         (uint32_t)stillBlack==BLACK) {
        int32_t back=set(cfg,DVOM,HI50);
        int64_t checkMode=0,checkIdle=0;
        restored=back==0&&!get(cfg,DVOM,&checkMode)&&
                 !get(cfg,VOIO,&checkIdle)&&(uint32_t)checkMode==HI50&&
                 (uint32_t)checkIdle==BLACK;
        printf("stage=hdmi_idle_input_differential volatile_rollback rc=0x%08x verified=%u\n",
               (unsigned)back,restored?1:0);
      } else {
        printf("stage=hdmi_idle_input_differential rollback=GUARDED_OUT "
               "mode_read_rc=0x%08x idle_read_rc=0x%08x observed_mode=0x%08x "
               "observed_idle=0x%08x\n",
               (unsigned)checkModeRc,(unsigned)checkIdleRc,
               (unsigned)(uint32_t)actual,(unsigned)(uint32_t)stillBlack);
      }
      if(!restored)result=50;
    } else if(!result) {
      // The first probe may have failed before any configuration write.
      int64_t actual=0,stillBlack=0;
      restored=!get(cfg,DVOM,&actual)&&!get(cfg,VOIO,&stillBlack)&&
               (uint32_t)actual==HI50&&(uint32_t)stillBlack==BLACK;
    }
    if(restored&&!interrupted) {
      a2=probe(input,status,"A2");
      if(!result&&(a2.enableRc||a2.disableRc||!a2.observed))result=28;
    }
    printf("stage=hdmi_idle_input_differential result=%s "
           "A1_lock=%u B_lock=%u A2_lock=%u restored_Hi50=%u "
           "persisted=0 streams_started=0 frames_recorded=0\n",
           result?"INCOMPLETE":b.locked?"LOCK_IN_B":"NO_LOCK_IN_B",
           a1.locked?1:0,b.locked?1:0,a2.locked?1:0,restored?1:0);
    if(!result)result=b.locked?0:42;
  }
  release(status);release(input);release(cfg);release(dev);dlclose(lib);
  return interrupted?130:result;
}
