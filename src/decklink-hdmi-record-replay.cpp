// Fountain Coach — fixed-silver Blackmagic HDMI recording/playback round trip.
// The ONLY physical input source: silver Macmini5,3 HDMI OUT -> original
// Intensity Extreme HDMI IN -> same Sierra Mac through Thunderbolt 1.
// Extension: original Intensity Extreme HDMI OUT -> user's external monitor.
//
// The old Desktop Video 10.4 GetStreamTime accessor must NOT be called.
// We capture 30 FULL frames, stop input, privately record exact packed UYVY,
// then replay those exact recorded bytes at 1080p30 via DisplayVideoFrameSync.
// No PCI writes, firmware, KEXT, re-cabling, other source or guest changes.
//
// This test can verify exact digital recording -> output-buffer identity and
// API acceptance. A person observing the attached HDMI monitor independently
// must witness optical playback. An S_OK API return is NOT optical proof.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <vector>
#include <CommonCrypto/CommonDigest.h>
#include <mach/mach_time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int kWidth=1920, kHeight=1080, kStride=3840;
static const size_t kFrameBytes=(size_t)kHeight*kStride;
static const int kFrames=30;
static const uint32_t kMode1080p30=0x48703330u;  // Hp30
static const uint32_t kUYVY=0x32767579u;         // 2vuy
static const char kStoreBase[]="/Volumes/Fresh/FountainVideoDockerLab/private-roundtrip";
static const char kApiPath[]="/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI";
static const uint8_t kInputIID[16]={0xaf,0x22,0x76,0x2b,0xdf,0xac,0x48,0x46,
                                      0xaa,0x79,0xfa,0x88,0x83,0x56,0x09,0x95};
static const uint8_t kOutputIID[16]={0xcc,0x5c,0x8a,0x6e,0x3f,0x2f,0x4b,0x3a,
                                      0x87,0xea,0xfd,0x78,0xaf,0x30,0x05,0x64};
static const uint8_t kStatusIID[16]={0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,
                                      0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
static const uint8_t kCallbackIID[16]={0xdd,0x04,0xe5,0xec,0x74,0x15,0x42,0xab,
                                        0xae,0x4a,0xe8,0x0c,0x4d,0xfc,0x04,0x4a};
static const uint8_t kUnknownIID[16]={0,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};

typedef void*(*Factory)();
typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef int32_t(*Next)(void*,void**);
typedef uint32_t(*Release)(void*);
typedef int32_t(*GetFlag)(void*,uint32_t,bool*);
typedef int32_t(*EnableIn)(void*,uint32_t,uint32_t,uint32_t);
typedef int32_t(*EnableOut)(void*,uint32_t,uint32_t);
typedef int32_t(*NoArg)(void*);
typedef int32_t(*SetCallback)(void*,void*);
typedef uint32_t(*Flags)(void*);
typedef long(*Dimension)(void*);
typedef int32_t(*FrameBytes)(void*,void**);
typedef int32_t(*CreateFrame)(void*,int32_t,int32_t,int32_t,uint32_t,uint32_t,void**);
typedef int32_t(*DisplaySync)(void*,void*);
typedef int32_t(*GetModeIterator)(void*,void**);
typedef uint32_t(*ModeCode)(void*);
typedef int32_t(*DoesSupport)(void*,uint32_t,uint32_t,uint32_t,uint32_t*,void**);

static void releaseObject(void* p) {
    if(p) ((Release)(*(void***)p)[2])(p);
}
static int32_t query(void* object,const uint8_t* raw,void** out) {
    if(!object || !out)return (int32_t)0x80004003u;
    CFUUIDBytes id; memcpy(&id,raw,16);
    return ((QI)(*(void***)object)[0])(object,id,out);
}
static uint64_t clockNs() {
    static mach_timebase_info_data_t tb={0,0};
    if(tb.denom==0 && mach_timebase_info(&tb)!=KERN_SUCCESS)return 0;
    if(!tb.denom)return 0;
    return (uint64_t)(((__uint128_t)mach_absolute_time()*tb.numer)/tb.denom);
}
static void hexDigest(const uint8_t digest[CC_SHA256_DIGEST_LENGTH],
                      char out[CC_SHA256_DIGEST_LENGTH*2+1]) {
    static const char alphabet[]="0123456789abcdef";
    for(int i=0;i<CC_SHA256_DIGEST_LENGTH;++i) {
        out[i*2]=alphabet[digest[i]>>4];
        out[i*2+1]=alphabet[digest[i]&15];
    }
    out[CC_SHA256_DIGEST_LENGTH*2]=0;
}
static bool sameSilverHDMIRig() {
    char model[64]={0}; size_t n=sizeof(model);
    if(sysctlbyname("hw.model",model,&n,nullptr,0)!=0 ||
       strcmp(model,"Macmini5,3")) return false;
    CGDirectDisplayID displays[32]={0}; uint32_t found=0;
    if(CGGetOnlineDisplayList(32,displays,&found)!=kCGErrorSuccess)return false;
    for(uint32_t i=0;i<found;++i) {
        const CGDirectDisplayID display=displays[i];
        if(CGDisplayVendorNumber(display)!=8961 ||
           CGDisplayPixelsWide(display)!=kWidth ||
           CGDisplayPixelsHigh(display)!=kHeight)continue;
        CGDisplayModeRef m=CGDisplayCopyDisplayMode(display);
        const double hz=m?CGDisplayModeGetRefreshRate(m):0;
        if(m)CGDisplayModeRelease(m);
        if(hz>29.9 && hz<30.1) return true;
    }
    return false;
}
class DeckLinkDevice {
public:
    void* library=nullptr;
    void* device=nullptr;
    bool open() {
        library=dlopen(kApiPath,RTLD_NOW|RTLD_LOCAL);
        if(!library) {puts("stage=framework result=absent");return false;}
        Factory create=(Factory)dlsym(library,"CreateDeckLinkIteratorInstance");
        if(!create)return false;
        void* iterator=create();
        if(!iterator)return false;
        int32_t rc=((Next)(*(void***)iterator)[3])(iterator,&device);
        releaseObject(iterator);
        printf("stage=decklink_device enumerate_rc=0x%08x present=%d\n",
               (unsigned)rc,device!=nullptr);
        fflush(stdout);
        return rc==0 && device;
    }
    ~DeckLinkDevice() {
        releaseObject(device);
        if(library)dlclose(library);
    }
};

struct FrameRecord {
    uint8_t* data=nullptr;
    uint64_t hostNs=0;
    uint8_t digest[CC_SHA256_DIGEST_LENGTH]={0};
    char digestHex[CC_SHA256_DIGEST_LENGTH*2+1]={0};
    bool committed=false;
};
static FrameRecord gRecorded[kFrames];
static std::atomic<int> gClaimed(0),gReady(0),gNoSource(0),gInvalid(0);
struct Callback {
    void** vt;
    std::atomic<uint32_t> refs,seen;
    Callback():vt(nullptr),refs(1),seen(0){}
};
static uint32_t cAdd(Callback* c){return ++c->refs;}
static uint32_t cDrop(Callback* c){return --c->refs;}
static int32_t cQuery(Callback* c,CFUUIDBytes iid,void** out) {
    if(!out)return (int32_t)0x80004003u;
    if(!memcmp(&iid,kCallbackIID,16)||!memcmp(&iid,kUnknownIID,16)) {
        *out=c;cAdd(c);return 0;
    }
    *out=nullptr;return (int32_t)0x80004002u;
}
static int32_t cMode(Callback*,uint32_t,void*,uint32_t){return 0;}
static int32_t cFrame(Callback* c,void* frame,void*) {
    ++c->seen;
    if(!frame) return 0;
    void** vt=*(void***)frame;
    uint32_t flags=((Flags)vt[7])(frame);
    if(flags&0x80000000u) {++gNoSource;return 0;}
    if(((Dimension)vt[3])(frame)!=kWidth ||
       ((Dimension)vt[4])(frame)!=kHeight ||
       ((Dimension)vt[5])(frame)!=kStride) {++gInvalid;return 0;}
    if(gReady.load(std::memory_order_relaxed)>=kFrames)return 0;
    void* borrowed=nullptr;
    if(((FrameBytes)vt[8])(frame,&borrowed)!=0||!borrowed) {
        ++gInvalid;return 0;
    }
    const int idx=gClaimed.fetch_add(1,std::memory_order_relaxed);
    if(idx>=kFrames)return 0;
    // Borrowed memory is NEVER read again after callback returns.
    memcpy(gRecorded[idx].data,borrowed,kFrameBytes);
    gRecorded[idx].hostNs=clockNs();       // not device-native PTS
    gRecorded[idx].committed=true;
    gReady.fetch_add(1,std::memory_order_release);
    return 0;
}
static void* kCallbackVtable[5]={
    (void*)cQuery,(void*)cAdd,(void*)cDrop,(void*)cMode,(void*)cFrame
};
static bool captureRecording(DeckLinkDevice& deck) {
    void* input=nullptr;void* status=nullptr;
    int rc=query(deck.device,kInputIID,&input);
    printf("stage=input_interface rc=0x%08x present=%d\n",
           (unsigned)rc,input!=nullptr);
    if(rc||!input)return false;
    bool enabled=false,installed=false,started=false,okay=true;
    Callback callback;callback.vt=kCallbackVtable;
    void** iv=*(void***)input;
    rc=((EnableIn)iv[6])(input,kMode1080p30,kUYVY,0);
    printf("stage=enable_input rc=0x%08x mode=1080p30 pixel=2vuy\n",(unsigned)rc);
    if(rc){okay=false;goto finish;}
    enabled=true;
    rc=query(deck.device,kStatusIID,&status);
    if(rc||!status){okay=false;goto finish;}
    {
        void** sv=*(void***)status;
        int consecutive=0;
        for(int attempt=0;attempt<12&&consecutive<2;++attempt) {
            bool locked=false;
            int32_t lrc=((GetFlag)sv[3])(status,0x7669736c,&locked);
            if(lrc==0&&locked)++consecutive;
            else consecutive=0;
            printf("stage=input_lock sample=%d rc=0x%08x locked=%s\n",
                   attempt,(unsigned)lrc,lrc==0?(locked?"yes":"no"):"unknown");
            if(consecutive<2)usleep(250000);
        }
        if(consecutive<2){okay=false;puts("stage=input_lock result=no_stable_lock");goto finish;}
    }
    rc=((SetCallback)iv[17])(input,&callback);
    if(rc){okay=false;goto finish;}
    installed=true;
    rc=((NoArg)iv[13])(input);
    printf("stage=start_input rc=0x%08x\n",(unsigned)rc);
    if(rc){okay=false;goto finish;}
    started=true;
    {
        const uint64_t deadline=clockNs()+4500000000ull;
        while(gReady.load(std::memory_order_acquire)<kFrames &&
              clockNs()<deadline)usleep(20000);
    }
    if(gReady.load(std::memory_order_acquire)<kFrames)okay=false;
finish:
    // Stop/release input fully BEFORE enabling the device's HDMI OUT.
    if(started) {
        rc=((NoArg)iv[14])(input);
        printf("stage=stop_input rc=0x%08x\n",(unsigned)rc);
        if(rc)okay=false;
    }
    if(installed) {
        rc=((SetCallback)iv[17])(input,nullptr);
        printf("stage=detach_input_callback rc=0x%08x\n",(unsigned)rc);
        if(rc)okay=false;
    }
    if(enabled) {
        rc=((NoArg)iv[7])(input);
        printf("stage=disable_input rc=0x%08x\n",(unsigned)rc);
        if(rc)okay=false;
    }
    releaseObject(status);
    releaseObject(input);
    printf("stage=recording_capture callbacks=%u frames=%d no_source=%d invalid=%d complete=%d\n",
           callback.seen.load(),gReady.load(),gNoSource.load(),
           gInvalid.load(),gReady.load()==kFrames && okay);
    fflush(stdout);
    return okay&&gReady.load()==kFrames;
}
static bool writeExact(int fd,const uint8_t* data,size_t len) {
    size_t done=0;
    while(done<len) {
        ssize_t n=write(fd,data+done,len-done);
        if(n<=0)return false;
        done+=(size_t)n;
    }
    return true;
}
static bool readExact(int fd,uint8_t* data,size_t len) {
    size_t done=0;
    while(done<len) {
        ssize_t n=read(fd,data+done,len-done);
        if(n<=0)return false;
        done+=(size_t)n;
    }
    return true;
}
static bool makePrivateDir(char out[512]) {
    struct stat st;
    if(lstat(kStoreBase,&st)!=0) {
        if(errno!=ENOENT || mkdir(kStoreBase,0700)!=0)return false;
        if(lstat(kStoreBase,&st)!=0)return false;
    }
    if(!S_ISDIR(st.st_mode)||st.st_uid!=getuid()||(st.st_mode&0077))return false;
    if(snprintf(out,512,"%s/recording-XXXXXX",kStoreBase)>=512)return false;
    return mkdtemp(out)!=nullptr;
}
static void framePath(const char* dir,int i,char out[640]) {
    snprintf(out,640,"%s/frame-%02d.uyvy",dir,i);
}
static bool saveRecording(const char* dir) {
    char manifest[640]={0};
    snprintf(manifest,sizeof(manifest),"%s/manifest.tsv",dir);
    int meta=open(manifest,O_WRONLY|O_CREAT|O_EXCL,0600);
    if(meta<0)return false;
    bool ok=true;
    const char intro[]="index\twidth\theight\trow_bytes\thost_monotonic_ns\tbytes\tsha256\n";
    if(!writeExact(meta,(const uint8_t*)intro,strlen(intro)))ok=false;
    for(int i=0;i<kFrames&&ok;++i) {
        if(!gRecorded[i].committed){ok=false;break;}
        CC_SHA256(gRecorded[i].data,(CC_LONG)kFrameBytes,gRecorded[i].digest);
        hexDigest(gRecorded[i].digest,gRecorded[i].digestHex);
        char name[640];framePath(dir,i,name);
        int fd=open(name,O_WRONLY|O_CREAT|O_EXCL,0600);
        if(fd<0){ok=false;break;}
        if(!writeExact(fd,gRecorded[i].data,kFrameBytes)||fsync(fd)!=0)ok=false;
        if(close(fd)!=0)ok=false;
        char line[256];
        int n=snprintf(line,sizeof(line),"%d\t1920\t1080\t3840\t%llu\t%lu\t%s\n",
                       i,(unsigned long long)gRecorded[i].hostNs,
                       (unsigned long)kFrameBytes,gRecorded[i].digestHex);
        if(n<0||n>=(int)sizeof(line)||!writeExact(meta,(const uint8_t*)line,(size_t)n))ok=false;
    }
    if(fsync(meta)!=0)ok=false;
    if(close(meta)!=0)ok=false;
    printf("stage=private_recording result=%s frames=%d bytes=%lu location=%s "
           "sierra_user_only=1 published=0 source_device_timestamp_used=0\n",
           ok?"recorded":"FAILED",kFrames,(unsigned long)(kFrames*kFrameBytes),dir);
    fflush(stdout);
    return ok;
}
static bool outputModeAvailable(void* output) {
    void** v=*(void***)output;
    uint32_t support=0;
    void* actual=nullptr;
    int32_t rc=((DoesSupport)v[3])(output,kMode1080p30,kUYVY,0,&support,&actual);
    if(actual)releaseObject(actual);
    printf("stage=output_mode_probe mode=1080p30 pixel=2vuy "
           "rc=0x%08x support=%u\n",(unsigned)rc,support);
    fflush(stdout);
    return rc==0 && support!=0;
}
static bool cueBars(void* output,unsigned seconds) {
    void** ov=*(void***)output;
    void* frame=nullptr;
    int32_t rc=((CreateFrame)ov[9])(output,kWidth,kHeight,kStride,kUYVY,0,&frame);
    if(rc||!frame)return false;
    void* raw=nullptr;
    bool ok=((FrameBytes)(*(void***)frame)[8])(frame,&raw)==0 && raw;
    if(ok){
        const uint8_t bars[8][3]={
            {128,235,128},{16,210,146},{166,170,16},{54,145,34},
            {202,106,222},{90,81,240},{240,41,110},{128,16,128}
        };
        uint8_t* p=(uint8_t*)raw;
        for(int y=0;y<kHeight;++y)for(int pair=0;pair<kWidth/2;++pair) {
            const int color=pair/120;
            uint8_t* at=p+(size_t)y*kStride+(size_t)pair*4;
            at[0]=bars[color][0];at[1]=bars[color][1];
            at[2]=bars[color][2];at[3]=bars[color][1];
        }
        rc=((DisplaySync)ov[11])(output,frame);
        printf("stage=cue_bars rc=0x%08x seconds=%u\n",(unsigned)rc,seconds);
        ok=rc==0;
        if(ok)sleep(seconds);
    }
    releaseObject(frame);
    return ok;
}
static bool replayRecording(DeckLinkDevice& deck,const char* dir) {
    void* output=nullptr;
    int32_t rc=query(deck.device,kOutputIID,&output);
    printf("stage=output_interface rc=0x%08x present=%d\n",
           (unsigned)rc,output!=nullptr);
    if(rc||!output)return false;
    void** ov=*(void***)output;
    bool enabled=false,ok=outputModeAvailable(output);
    int accepted=0,digitalMatches=0;
    if(!ok)goto done;
    rc=((EnableOut)ov[6])(output,kMode1080p30,0);
    printf("stage=enable_output rc=0x%08x mode=1080p30\n",(unsigned)rc);
    if(rc){ok=false;goto done;}
    enabled=true;
    if(!cueBars(output,1)){ok=false;goto done;}
    for(int repeat=0;repeat<2 && ok;++repeat) {
        for(int i=0;i<kFrames && ok;++i) {
            void* frame=nullptr;
            rc=((CreateFrame)ov[9])(output,kWidth,kHeight,kStride,kUYVY,0,&frame);
            if(rc||!frame){ok=false;break;}
            void* bytes=nullptr;
            rc=((FrameBytes)(*(void***)frame)[8])(frame,&bytes);
            if(rc||!bytes){ok=false;releaseObject(frame);break;}
            char path[640];framePath(dir,i,path);
            int fd=open(path,O_RDONLY|O_NOFOLLOW);
            if(fd<0 || !readExact(fd,(uint8_t*)bytes,kFrameBytes))ok=false;
            if(fd>=0 && close(fd)!=0)ok=false;
            uint8_t checksum[CC_SHA256_DIGEST_LENGTH];
            if(ok) {
                CC_SHA256(bytes,(CC_LONG)kFrameBytes,checksum);
                if(memcmp(checksum,gRecorded[i].digest,sizeof(checksum)))ok=false;
                else ++digitalMatches;
            }
            if(ok) {
                rc=((DisplaySync)ov[11])(output,frame);
                if(rc==0)++accepted;
                else {ok=false;printf("stage=hardware_output_error rc=0x%08x index=%d\n",
                                      (unsigned)rc,i);}
            }
            releaseObject(frame);
            if(ok)usleep(33333);  // 1080p30 pacing; not device-derived PTS
        }
    }
    if(ok && !cueBars(output,1))ok=false;
done:
    if(enabled) {
        rc=((NoArg)ov[7])(output);
        printf("stage=disable_output rc=0x%08x\n",(unsigned)rc);
        if(rc)ok=false;
    }
    releaseObject(output);
    printf("stage=replay result=%s recorded_frames=%d submitted=%d "
           "byte_identical_output_buffers=%d visual_monitor_witness=NOT_MEASURED\n",
           ok&&accepted==kFrames*2&&digitalMatches==accepted?"api_accepted":"FAILED",
           kFrames,accepted,digitalMatches);
    fflush(stdout);
    return ok&&accepted==kFrames*2&&digitalMatches==accepted;
}
// Fallback optical playback when today's input currently has no signal lock.
// Both private JPEGs were captured earlier from the *same* physical
// Blackmagic HDMI IN; this does not manufacture a new recording. The decoded
// content is upscaled to the exact same UYVY 1080p30 output interface.
static int clampByte(int n) {return n<0?0:n>255?255:n;}
static void convertRGB(const uint8_t* pixel,int& Y,int& U,int& V) {
    const int R=pixel[0],G=pixel[1],B=pixel[2];
    Y=clampByte(16+((47*R+157*G+16*B+128)>>8));
    U=clampByte(128+((-26*R-87*G+112*B+128)>>8));
    V=clampByte(128+((112*R-102*G-10*B+128)>>8));
}
static bool openArchivedJPEG(int which,std::vector<uint8_t>& dest,char shaHex[65]) {
    const char* name=which==0?"/tmp/fc-intensity-extreme-physical-frame0.jpg":
                              "/tmp/fc-intensity-extreme-physical-frame1.jpg";
    const char* reference=which==0?
        "ea1f51836311a496d91f493f82bb9e380f41efff76992ef8438cea6511c562d3":
        "ab1abfddb80eb4a7d1404272aefed88d55373fd344dbb1debdc63a19c0c32634";
    struct stat st;
    if(lstat(name,&st)||!S_ISREG(st.st_mode)||st.st_uid!=getuid()||
       (st.st_mode&0077)!=0||st.st_nlink!=1||
       st.st_size<100||st.st_size>262144)return false;
    const int fd=open(name,O_RDONLY|O_NOFOLLOW);
    if(fd<0)return false;
    std::vector<uint8_t> jpeg((size_t)st.st_size);
    const bool all=readExact(fd,jpeg.data(),jpeg.size());
    close(fd);
    if(!all)return false;
    uint8_t signature[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(jpeg.data(),(CC_LONG)jpeg.size(),signature);
    hexDigest(signature,shaHex);
    if(strcmp(shaHex,reference)!=0)return false;
    CFURLRef url=CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,(const UInt8*)name,strlen(name),false);
    if(!url)return false;
    CGImageSourceRef source=CGImageSourceCreateWithURL(url,nullptr);
    CFRelease(url);
    if(!source)return false;
    CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,nullptr);
    CFRelease(source);
    if(!image)return false;
    const size_t srcW=320,srcH=180;
    if(CGImageGetWidth(image)!=srcW || CGImageGetHeight(image)!=srcH){
        CGImageRelease(image);return false;
    }
    std::vector<uint8_t> rgba(srcW*srcH*4,0);
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    if(!space){CGImageRelease(image);return false;}
    CGContextRef ctx=CGBitmapContextCreate(rgba.data(),srcW,srcH,8,
                      srcW*4,space,kCGImageAlphaPremultipliedLast|
                      kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);
    if(!ctx){CGImageRelease(image);return false;}
    CGContextSetInterpolationQuality(ctx,kCGInterpolationNone);
    CGContextDrawImage(ctx,CGRectMake(0,0,srcW,srcH),image);
    CGContextRelease(ctx);
    CGImageRelease(image);
    dest.resize(kFrameBytes);
    for(int y=0;y<kHeight;++y) {
        uint8_t* dst=dest.data()+(size_t)y*kStride;
        const size_t sy=(size_t)y*srcH/kHeight;
        for(int pair=0;pair<kWidth/2;++pair) {
            const size_t x0=(size_t)(pair*2)*srcW/kWidth;
            const size_t x1=(size_t)(pair*2+1)*srcW/kWidth;
            const uint8_t* a=rgba.data()+(sy*srcW+x0)*4;
            const uint8_t* b=rgba.data()+(sy*srcW+x1)*4;
            int y0,u0,v0,y1,u1,v1;
            convertRGB(a,y0,u0,v0);
            convertRGB(b,y1,u1,v1);
            uint8_t* p=dst+(size_t)pair*4;
            p[0]=(uint8_t)((u0+u1+1)/2);
            p[1]=(uint8_t)y0;
            p[2]=(uint8_t)((v0+v1+1)/2);
            p[3]=(uint8_t)y1;
        }
    }
    return true;
}
static bool replayArchivedPhysicalJPEGs(DeckLinkDevice& deck) {
    std::vector<uint8_t> frames[2];
    char originalSha[2][65]={{0}};
    if(!openArchivedJPEG(0,frames[0],originalSha[0]) ||
       !openArchivedJPEG(1,frames[1],originalSha[1])) {
        puts("stage=archive_playback result=missing_or_unverified_private_real_capture");
        return false;
    }
    void* output=nullptr;
    int32_t rc=query(deck.device,kOutputIID,&output);
    bool ok=rc==0&&output&&outputModeAvailable(output);
    if(!ok){releaseObject(output);return false;}
    void** ov=*(void***)output;
    rc=((EnableOut)ov[6])(output,kMode1080p30,0);
    printf("stage=archived_output_enable rc=0x%08x\n",(unsigned)rc);
    if(rc){releaseObject(output);return false;}
    int apiAccepted=0,byteIdentical=0;
    if(cueBars(output,1)) {
        for(int repeat=0;repeat<2&&ok;++repeat)for(int which=0;which<2&&ok;++which){
            void* frame=nullptr;
            rc=((CreateFrame)ov[9])(output,kWidth,kHeight,kStride,kUYVY,0,&frame);
            if(rc||!frame){ok=false;break;}
            void* outBytes=nullptr;
            rc=((FrameBytes)(*(void***)frame)[8])(frame,&outBytes);
            if(rc||!outBytes){ok=false;releaseObject(frame);break;}
            memcpy(outBytes,frames[which].data(),kFrameBytes);
            bool equals=memcmp(outBytes,frames[which].data(),kFrameBytes)==0;
            if(equals)++byteIdentical;
            if(equals) {
                rc=((DisplaySync)ov[11])(output,frame);
                if(!rc) ++apiAccepted;
                else ok=false;
            } else ok=false;
            printf("stage=archived_frame_playback source=original_24Oct_HMDI "
                   "recording_index=%d repeat=%d output_rc=0x%08x "
                   "full_output_buffer_matches_decoded_recording=%d "
                   "source_jpeg_sha256=%s\n",
                   which,repeat,(unsigned)rc,equals?1:0,originalSha[which]);
            fflush(stdout);
            releaseObject(frame);
            if(ok)sleep(2);
        }
    } else ok=false;
    if(ok&&!cueBars(output,1))ok=false;
    int32_t stop=((NoArg)ov[7])(output);
    printf("stage=archived_output_disable rc=0x%08x\n",(unsigned)stop);
    if(stop)ok=false;
    releaseObject(output);
    printf("stage=archived_playback result=%s actual_source_JPEGs=2 "
           "output_submissions=%d byte_identical_converted_buffers=%d "
           "optical_monitor_witness=NOT_MEASURED "
           "new_recording_accepted=0\n",
           ok&&apiAccepted==4&&byteIdentical==4?"API_ACCEPTED":"FAILED",
           apiAccepted,byteIdentical);
    return ok&&apiAccepted==4&&byteIdentical==4;
}
int main(int argc,char** argv) {
    const bool probe=argc==2&&!strcmp(argv[1],"--probe-output-1080p30");
    const bool roundtrip=argc==2&&!strcmp(argv[1],"--record-replay-1080p30");
    const bool cue=argc==2&&!strcmp(argv[1],"--monitor-cue-1080p30");
    const bool archive=argc==2&&!strcmp(argv[1],"--play-archived-physical-jpegs-1080p30");
    if(!probe&&!roundtrip&&!cue&&!archive) {
        fprintf(stderr,"usage: %s --probe-output-1080p30 | --monitor-cue-1080p30 | --record-replay-1080p30 | --play-archived-physical-jpegs-1080p30\n",argv[0]);
        return 64;
    }
    if(!sameSilverHDMIRig()) {
        puts("stage=fixed_silver_hdmi result=NO_GO wrong_host_or_BMD_1080p30");
        return 21;
    }
    printf("stage=physical_rig result=accepted_input_identity "
           "silver_hdmi_out_to_original_intensity_hdmi_in=1 "
           "intensity_hdmi_out_to_monitor=NEW_TEST "
           "sierra_host=unchanged guest_network=unchanged\n");
    puts("stage=safety vendor_GetStreamTime=0 "
         "driver_change=0 firmware_change=0 external_publication=0 "
         "simultaneous_capture_and_playback=0");
    fflush(stdout);
    DeckLinkDevice deck;
    if(!deck.open())return 22;
    if(probe) {
        void* output=nullptr;
        const int32_t rc=query(deck.device,kOutputIID,&output);
        printf("stage=output_interface rc=0x%08x present=%d "
               "active_output_enable_calls=0\n",(unsigned)rc,output!=nullptr);
        bool supported=rc==0&&output&&outputModeAvailable(output);
        releaseObject(output);
        printf("stage=output_preflight result=%s monitor_light_not_tested=1\n",
               supported?"PASS":"NOT_SUPPORTED");
        return supported?0:23;
    }
    if(archive) return replayArchivedPhysicalJPEGs(deck)?0:32;
    if(cue) {
        void* output=nullptr;
        int32_t rc=query(deck.device,kOutputIID,&output);
        bool okay=rc==0&&output&&outputModeAvailable(output);
        bool enabled=false;
        int32_t stop=0;
        if(okay){
            rc=((EnableOut)(*(void***)output)[6])(output,kMode1080p30,0);
            printf("stage=monitor_cue_enable rc=0x%08x\n",(unsigned)rc);
            enabled=rc==0;
            okay=enabled;
        }
        if(okay)okay=cueBars(output,6);
        if(enabled) {
            stop=((NoArg)(*(void***)output)[7])(output);
            printf("stage=monitor_cue_disable rc=0x%08x\n",(unsigned)stop);
            if(stop)okay=false;
        }
        releaseObject(output);
        printf("stage=monitor_cue result=%s mode=1080p30 "
               "output_API_accepted=%d optical_display_verified=0 "
               "HDMI_input_not_touched=1\n",okay?"API_ACCEPTED":"FAIL",okay);
        return okay?0:31;
    }
    for(int i=0;i<kFrames;++i) {
        gRecorded[i].data=(uint8_t*)malloc(kFrameBytes);
        if(!gRecorded[i].data) {
            puts("stage=recording result=allocation_failure");
            for(int j=0;j<=i;++j)free(gRecorded[j].data);
            return 24;
        }
    }
    char dir[512]={0};
    int result=0;
    const bool recorded=captureRecording(deck);
    if(!recorded)result=25;
    else if(!makePrivateDir(dir))result=26;
    else if(!saveRecording(dir))result=27;
    else if(!replayRecording(deck,dir))result=28;
    printf("stage=roundtrip result=%s capture_mode=1080p30 "
           "complete_recording_frames=%d "
           "digital_recording_to_output_buffer_match=%s "
           "hdmi_out_monitor_physical_display=UNWITNESSED "
           "recording_folder=%s\n",
           result?"NOT_ACCEPTED":"DIGITAL_ACCEPTED",gReady.load(),
           result?"not_accepted":"all_played_frames",dir[0]?dir:"none");
    for(int i=0;i<kFrames;++i)free(gRecorded[i].data);
    return result;
}
