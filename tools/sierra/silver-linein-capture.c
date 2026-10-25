/*
 * Fountain Coach / Silver Mac — exact analog LINE input recording witness.
 * Sierra 10.12.6, Macmini5,3, Core Audio AudioQueue.
 * Captures ONLY the physically selected Built-in Input device; no monitor,
 * no source changes, no system defaults changed, no Blackmagic output enable.
 * Private local WAV, never published. No simulated samples in the recording.
 */
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

typedef struct {
    FILE *file;
    volatile uint64_t frames;
    uint64_t target_frames;
    volatile int io_error;
    volatile int queue_error;
} Capture;

static void le16(FILE *f, unsigned v){
    fputc(v&255,f);fputc((v>>8)&255,f);
}
static void le32(FILE *f, uint32_t v){
    for(int k=0;k<4;k++)fputc((v>>(8*k))&255,f);
}
static void wav_header(FILE *f,unsigned rate,uint32_t data_size) {
    rewind(f);
    fwrite("RIFF",1,4,f);le32(f,data_size+36);
    fwrite("WAVEfmt ",1,8,f);le32(f,16);le16(f,1);
    le16(f,2);le32(f,rate);le32(f,rate*4);le16(f,4);le16(f,16);
    fwrite("data",1,4,f);le32(f,data_size);
}
static int silver_and_guard(void){
    char model[64]={0},os[64]={0};size_t m=sizeof(model),s=sizeof(os);
    if(sysctlbyname("hw.model",model,&m,NULL,0)!=0 ||
       sysctlbyname("kern.osrelease",os,&s,NULL,0)!=0 ||
       strcmp(model,"Macmini5,3") || strncmp(os,"16.",3)) return 0;
    const char *home=getenv("HOME");if(!home)return 0;
    char lock[1024];snprintf(lock,sizeof(lock),
       "%s/Library/Application Support/Fountain Coach/Video Instrument/run/OUTPUT_DISABLED",home);
    return access(lock,R_OK)==0;
}
static int select_input(AudioDeviceID *device,CFStringRef *uid) {
    *device=0;*uid=NULL;
    AudioObjectPropertyAddress a={kAudioHardwarePropertyDevices,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};
    UInt32 size=0;
    if(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,&a,0,NULL,&size)!=noErr||size==0)return 0;
    AudioDeviceID *devs=(AudioDeviceID*)calloc(1,size);
    if(!devs)return 0;
    if(AudioObjectGetPropertyData(kAudioObjectSystemObject,&a,0,NULL,&size,devs)!=noErr){free(devs);return 0;}
    for(UInt32 k=0;k<size/sizeof(AudioDeviceID);k++){
        AudioObjectPropertyAddress u={kAudioDevicePropertyDeviceUID,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};
        CFStringRef cfuid=NULL;UInt32 n=sizeof(cfuid);
        if(AudioObjectGetPropertyData(devs[k],&u,0,NULL,&n,&cfuid)!=noErr||!cfuid)continue;
        char name[400]={0};CFStringGetCString(cfuid,name,sizeof(name),kCFStringEncodingUTF8);
        if(strcmp(name,"AppleHDAEngineInput:1B,0,1,1:2")==0){*device=devs[k];*uid=cfuid;break;}
        CFRelease(cfuid);
    }
    free(devs);
    if(!*device||!*uid)return 0;
    AudioObjectPropertyAddress p={kAudioDevicePropertyJackIsConnected,kAudioDevicePropertyScopeInput,kAudioObjectPropertyElementMaster};
    UInt32 connected=0,n=sizeof(connected);
    OSStatus j=AudioObjectGetPropertyData(*device,&p,0,NULL,&n,&connected);
    p.mSelector=kAudioDevicePropertyDataSource;
    UInt32 source=0;n=sizeof(source);
    OSStatus ds=AudioObjectGetPropertyData(*device,&p,0,NULL,&n,&source);
    p.mSelector=kAudioDevicePropertyMute;
    UInt32 mute=0;n=sizeof(mute);
    OSStatus mt=AudioObjectGetPropertyData(*device,&p,0,NULL,&n,&mute);
    printf("stage=physical_input source=0x%08x jack=%u mute=%u rc=%d/%d/%d\n",
      source,connected,mute,(int)j,(int)ds,(int)mt);
    if(j!=noErr||ds!=noErr||mt!=noErr||connected!=1||mute!=0||
       source!=0x6c696e65U) return 0; /* 'line' */
    return 1;
}
static void input_callback(void *opaque,AudioQueueRef queue,
                            AudioQueueBufferRef buffer,const AudioTimeStamp *stamp,
                            UInt32 num_packets,const AudioStreamPacketDescription *desc){
    (void)stamp;(void)num_packets;(void)desc;
    Capture *c=(Capture*)opaque;
    if(!c||c->io_error||c->queue_error||c->frames>=c->target_frames)return;
    UInt32 have=buffer->mAudioDataByteSize/4;
    uint64_t remain=c->target_frames-c->frames;
    UInt32 n=(uint64_t)have<remain?have:(UInt32)remain;
    if(n){
        size_t bytes=(size_t)n*4;
        if(fwrite(buffer->mAudioData,1,bytes,c->file)!=bytes)c->io_error=1;
        else c->frames+=n;
    }
    if(c->frames<c->target_frames&&!c->io_error){
        OSStatus r=AudioQueueEnqueueBuffer(queue,buffer,0,NULL);
        if(r!=noErr)c->queue_error=(int)r;
    }
}
int main(int argc,char **argv) {
    if(argc==2 && strcmp(argv[1],"--inspect")==0){
        if(!silver_and_guard()) return 20;
        AudioDeviceID probe=0;CFStringRef probe_uid=NULL;
        int ok=select_input(&probe,&probe_uid);
        if(probe_uid)CFRelease(probe_uid);
        printf("stage=recorder_input_ready ready=%d\n",ok);
        return ok?0:21;
    }
    if(argc!=4||strcmp(argv[1],"--seconds")!=0){
        fprintf(stderr,"usage: silver-linein-capture --seconds <2..12> /tmp/<private>.wav\n");
        return 64;
    }
    char *end=NULL;long sec=strtol(argv[2],&end,10);
    const char *home=getenv("HOME");
    char private_prefix[1024]={0};
    if(home)snprintf(private_prefix,sizeof(private_prefix),
        "%s/Music/Fountain Coach/Die Maschine/Recordings/fc-",home);
    int path_allowed=(strncmp(argv[3],"/tmp/fc-",8)==0) ||
        (home && strlen(private_prefix)<sizeof(private_prefix)-1 &&
         strncmp(argv[3],private_prefix,strlen(private_prefix))==0);
    if(!end||*end||sec<2||sec>12||!path_allowed){
        puts("stage=arguments result=REFUSED");return 65;
    }
    if(!silver_and_guard()){puts("stage=preflight result=REFUSED_HOST_OR_GUARD");return 20;}
    AudioDeviceID device=0;CFStringRef uid=NULL;
    if(!select_input(&device,&uid)){puts("stage=preflight result=REFUSED_LINE_INPUT_NOT_CONNECTED");if(uid)CFRelease(uid);return 21;}
    AudioObjectPropertyAddress p={kAudioDevicePropertyNominalSampleRate,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};
    Float64 nominal=0;UInt32 n=sizeof(nominal);
    if(AudioObjectGetPropertyData(device,&p,0,NULL,&n,&nominal)!=noErr ||
       nominal<40000||nominal>96000){CFRelease(uid);puts("stage=rate result=REFUSED");return 22;}
    unsigned rate=(unsigned)nominal;
    AudioStreamBasicDescription fmt;memset(&fmt,0,sizeof(fmt));
    fmt.mSampleRate=rate;fmt.mFormatID=kAudioFormatLinearPCM;
    fmt.mFormatFlags=kLinearPCMFormatFlagIsSignedInteger|kLinearPCMFormatFlagIsPacked;
    fmt.mFramesPerPacket=1;fmt.mChannelsPerFrame=2;fmt.mBitsPerChannel=16;
    fmt.mBytesPerFrame=4;fmt.mBytesPerPacket=4;
    AudioQueueRef queue=NULL;Capture capture;memset(&capture,0,sizeof(capture));
    capture.target_frames=(uint64_t)sec*rate;
    OSStatus rc=AudioQueueNewInput(&fmt,input_callback,&capture,NULL,NULL,0,&queue);
    if(rc!=noErr||!queue){printf("stage=queue result=FAIL code=%d\n",(int)rc);CFRelease(uid);return 23;}
    rc=AudioQueueSetProperty(queue,kAudioQueueProperty_CurrentDevice,&uid,sizeof(uid));
    if(rc!=noErr){printf("stage=set_exact_input result=FAIL code=%d\n",(int)rc);
        AudioQueueDispose(queue,true);CFRelease(uid);return 24;}
    CFStringRef routed=NULL;n=sizeof(routed);
    rc=AudioQueueGetProperty(queue,kAudioQueueProperty_CurrentDevice,&routed,&n);
    int exact=rc==noErr&&routed&&CFEqual(uid,routed);
    if(routed)CFRelease(routed);CFRelease(uid);
    if(!exact){puts("stage=route result=REFUSED_MISMATCH");AudioQueueDispose(queue,true);return 25;}
    int fd=open(argv[3],O_CREAT|O_EXCL|O_WRONLY,0600);
    if(fd<0){printf("stage=outfile result=FAIL errno=%d\n",errno);AudioQueueDispose(queue,true);return 26;}
    capture.file=fdopen(fd,"wb");
    if(!capture.file){close(fd);AudioQueueDispose(queue,true);return 27;}
    wav_header(capture.file,rate,0);
    const unsigned bufsize=4096*4;
    for(unsigned i=0;i<8;i++){
        AudioQueueBufferRef buf=NULL;rc=AudioQueueAllocateBuffer(queue,bufsize,&buf);
        if(rc!=noErr||!buf){puts("stage=allocate result=FAIL");goto fail;}
        rc=AudioQueueEnqueueBuffer(queue,buf,0,NULL);
        if(rc!=noErr){puts("stage=enqueue result=FAIL");goto fail;}
    }
    printf("stage=recording_start exact_device=1 rate=%u channels=2 seconds=%ld file=%s\n",
            rate,sec,argv[3]);fflush(stdout);
    rc=AudioQueueStart(queue,NULL);
    if(rc!=noErr){printf("stage=start result=FAIL code=%d\n",(int)rc);goto fail;}
    for(long tick=0;tick<sec*10+30;tick++){
        usleep(100000);
        if(capture.frames>=capture.target_frames||capture.io_error||capture.queue_error)break;
    }
    rc=AudioQueueStop(queue,true);
    AudioQueueDispose(queue,true);
    fflush(capture.file);
    uint64_t frames=capture.frames;
    if(frames>UINT32_MAX/4){puts("stage=recording result=FAIL_SIZE");fclose(capture.file);return 29;}
    wav_header(capture.file,rate,(uint32_t)(frames*4));
    int error=ferror(capture.file);fclose(capture.file);
    printf("stage=recording_end frames=%llu target_frames=%llu bytes=%llu stop_code=%d io_error=%d queue_error=%d file_error=%d\n",
       (unsigned long long)frames,(unsigned long long)capture.target_frames,
       (unsigned long long)(frames*4+44),(int)rc,capture.io_error,capture.queue_error,error);
    return (frames==capture.target_frames&&!error&&!capture.io_error&&!capture.queue_error&&rc==noErr)?0:30;
fail:
    AudioQueueStop(queue,true);AudioQueueDispose(queue,true);fclose(capture.file);
    puts("stage=recording result=FAILED");
    return 31;
}
