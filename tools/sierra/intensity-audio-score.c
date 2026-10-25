/*
 * Fountain Coach — Intensity Extreme audio-only, native CoreAudio experiment.
 * Makes a quiet timed score using a single explicitly selected HAL device.
 * --speaker: Mac mini Built-in Output (speaker or analog headphone jack)
 * --phonic: Mac mini analog stereo -> Phonic mini-stereo 2T Return, quiet 8s
 * --blackmagic: genuine DeckLink DaisyCutter output engine over Thunderbolt
 *
 * No system default switching, no HDMI enable, no device configuration,
 * no audio input assumption, no video playback. No external packages.
 * Actual electrical/optical output at detached hardware ports unmeasured.
 */
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/sysctl.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>

#define CHANNELS 2
#define SEGMENTS 8
#define DURATION 2.4
static double g_duration=DURATION;
#define MAX_GAIN 0.095
static double g_gain=MAX_GAIN;

static uint32_t g_output_mode=0;
static uint32_t read_original_output_mode(void){
    FILE *p=popen("/tmp/fc-intensity-status-readonly-20261025","r");
    if(!p)return 0;
    char line[512];uint32_t code=0;
    while(fgets(line,sizeof(line),p)){
        char *start=strstr(line,"key=current_output_mode rc=0x00000000 value=0x");
        if(start){
            char *hex=strstr(start,"value=0x");
            if(hex)code=(uint32_t)strtoul(hex+8,NULL,16);
        }
    }
    if(pclose(p)!=0)return 0;
    return code;
}

typedef struct {
    volatile uint32_t returned_buffers;
    volatile uint64_t returned_frames;
    uint32_t frames_per_buffer;
} PlayState;

static void finished(void *raw, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    (void)queue;(void)buffer;
    PlayState *s=(PlayState*)raw;
    s->returned_buffers++;
    s->returned_frames += s->frames_per_buffer;
}
static void readable(CFStringRef s, char *out, size_t size) {
    if(!s || !CFStringGetCString(s,out,size,kCFStringEncodingUTF8))
        snprintf(out,size,"UNKNOWN");
}
static CFStringRef property_string(AudioObjectID id, AudioObjectPropertySelector selector) {
    AudioObjectPropertyAddress a={selector,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};
    CFStringRef s=NULL; UInt32 n=sizeof(s);
    return AudioObjectGetPropertyData(id,&a,0,NULL,&n,&s)==noErr?s:NULL;
}
static uint32_t channels(AudioDeviceID id, AudioObjectPropertyScope scope) {
    AudioObjectPropertyAddress a={kAudioDevicePropertyStreamConfiguration,scope,kAudioObjectPropertyElementMaster};
    UInt32 size=0;
    if(AudioObjectGetPropertyDataSize(id,&a,0,NULL,&size)!=noErr||!size)return 0;
    AudioBufferList *b=(AudioBufferList*)calloc(1,size);
    if(!b)return 0;
    uint32_t n=0;
    if(AudioObjectGetPropertyData(id,&a,0,NULL,&size,b)==noErr)
        for(UInt32 i=0;i<b->mNumberBuffers;i++)n+=b->mBuffers[i].mNumberChannels;
    free(b);return n;
}
static double sample_rate(AudioDeviceID id) {
    AudioObjectPropertyAddress a={kAudioDevicePropertyNominalSampleRate,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};
    Float64 rate=0;UInt32 n=sizeof(rate);
    return AudioObjectGetPropertyData(id,&a,0,NULL,&n,&rate)==noErr?rate:0;
}
static int original_host(void) {
    char model[64]={0},os[64]={0};
    size_t m=sizeof(model),o=sizeof(os);
    return sysctlbyname("hw.model",model,&m,NULL,0)==0 &&
           sysctlbyname("kern.osrelease",os,&o,NULL,0)==0 &&
           strcmp(model,"Macmini5,3")==0 && strncmp(os,"16.",3)==0;
}
static int guard_present(void) {
    const char *home=getenv("HOME");
    if(!home || strlen(home)>1024)return 0;
    char p[PATH_MAX];
    snprintf(p,sizeof(p),"%s/Library/Application Support/Fountain Coach/Video Instrument/run/OUTPUT_DISABLED",home);
    return access(p,R_OK)==0;
}
static AudioDeviceID select_device(const char *which, CFStringRef *uid_out) {
    *uid_out=NULL;
    AudioObjectPropertyAddress a={kAudioHardwarePropertyDevices,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};
    UInt32 size=0;
    if(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,&a,0,NULL,&size)!=noErr || !size) return 0;
    AudioDeviceID *devices=(AudioDeviceID*)calloc(1,size);
    if(!devices)return 0;
    if(AudioObjectGetPropertyData(kAudioObjectSystemObject,&a,0,NULL,&size,devices)!=noErr){free(devices);return 0;}
    AudioDeviceID found=0;
    for(UInt32 i=0;i<size/sizeof(AudioDeviceID);i++){
        CFStringRef name=property_string(devices[i],kAudioObjectPropertyName);
        CFStringRef uid=property_string(devices[i],kAudioDevicePropertyDeviceUID);
        char n[200],u[500];readable(name,n,sizeof(n));readable(uid,u,sizeof(u));
        int valid=channels(devices[i],kAudioDevicePropertyScopeOutput)>=CHANNELS;
        int match=((!strcmp(which,"speaker") || !strcmp(which,"phonic")) && !strcmp(n,"Built-in Output") && strstr(u,"AppleHDAEngineOutput:")==u) ||
                  (!strcmp(which,"blackmagic") && !strcmp(n,"Blackmagic Audio") &&
                   strstr(u,"com_blackmagic_design_iokit_DeckLink_DaisyCutterAudioOutputEngine:")==u);
        if(valid&&match && !found){
            found=devices[i];*uid_out=uid;if(uid)CFRetain(uid);
        }
        if(name)CFRelease(name);
        if(uid)CFRelease(uid);
    }
    free(devices);return found;
}
static void fill_segment(AudioQueueBufferRef b, unsigned segment, unsigned frames, double rate) {
    short *data=(short*)b->mAudioData;
    const double two_pi=6.283185307179586;
    static const double ratios[]={1.0,1.25,1.125,1.5};
    double fundamental=180.0+(double)(g_output_mode & 31U)*1.8;
    double note=fundamental*ratios[segment%4];
    for(unsigned i=0;i<frames;i++){
        uint64_t absolute=(uint64_t)segment*frames+i;
        double t=(double)absolute/rate;
        double total=g_duration,env=1.0;
        if(t<0.03)env=t/0.03;
        if(total-t<0.08)env=fmax(0,(total-t)/0.08);
        double gate=(fmod(t,0.4)<0.26)?1.0:0.28;
        double shift=(segment%2==0)?0.22:0.78;
        double tone=0.66*sin(two_pi*note*t) +
                    0.25*sin(two_pi*note*1.5*t) +
                    0.09*sin(two_pi*note*2.0*t);
        double v=tone*env*gate*g_gain;
        data[2*i]  =(short)(32767*v*(1.0-shift*0.48));
        data[2*i+1]=(short)(32767*v*(0.52+shift*0.48));
    }
    b->mAudioDataByteSize=frames*CHANNELS*sizeof(short);
}
static UInt32 device_running(AudioDeviceID id){
    AudioObjectPropertyAddress a={kAudioDevicePropertyDeviceIsRunning,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMaster};
    UInt32 running=0,n=sizeof(running);
    return AudioObjectGetPropertyData(id,&a,0,NULL,&n,&running)==noErr?running:UINT_MAX;
}
int main(int argc,char **argv) {
    if(argc!=2 || (strcmp(argv[1],"--speaker") && strcmp(argv[1],"--blackmagic") && strcmp(argv[1],"--phonic"))){
        fprintf(stderr,"usage: intensity-audio-score --speaker | --blackmagic | --phonic\n");return 64;
    }
    const char *which=argv[1]+2;
    if(!strcmp(which,"phonic")) { g_gain=0.020; g_duration=8.0; }
    if(!original_host()||!guard_present()){
        puts("stage=preflight result=REFUSED_HOST_OR_OUTPUT_GUARD");return 20;
    }
    g_output_mode=read_original_output_mode();
    if(!g_output_mode){
        puts("stage=preflight result=REFUSED_MISSING_LIVE_DECKLINK_MODE");return 30;
    }
    printf("stage=score_seed live_decklink_mode=0x%08x sound_source=Mac_synthesis\n",g_output_mode);
    CFStringRef uid=NULL;
    AudioDeviceID id=select_device(which,&uid);
    if(!id||!uid){puts("stage=select_device result=NO_EXACT_MATCH");return 21;}
    char name[500];readable(uid,name,sizeof(name));
    double rate=sample_rate(id);
    if(rate<40000||rate>192000){printf("stage=sample_rate result=OUT_OF_RANGE rate=%.1f\n",rate);CFRelease(uid);return 22;}
    printf("stage=target device_id=%u uid=%s output_channels=%u sample_rate=%.0f default_not_changed=1 guard=1\n",id,name,channels(id,kAudioDevicePropertyScopeOutput),rate);
    fflush(stdout);
    AudioStreamBasicDescription fmt; memset(&fmt,0,sizeof(fmt));
    fmt.mSampleRate=rate;
    fmt.mFormatID=kAudioFormatLinearPCM;
    fmt.mFormatFlags=kLinearPCMFormatFlagIsSignedInteger|kLinearPCMFormatFlagIsPacked;
    fmt.mBytesPerPacket=CHANNELS*sizeof(short);
    fmt.mFramesPerPacket=1;fmt.mBytesPerFrame=CHANNELS*sizeof(short);
    fmt.mChannelsPerFrame=CHANNELS;fmt.mBitsPerChannel=16;
    AudioQueueRef queue=NULL;PlayState state;memset(&state,0,sizeof(state));
    OSStatus rc=AudioQueueNewOutput(&fmt,finished,&state,NULL,NULL,0,&queue);
    if(rc!=noErr||!queue){printf("stage=create_queue result=FAIL code=%d\n",(int)rc);CFRelease(uid);return 23;}
    rc=AudioQueueSetProperty(queue,kAudioQueueProperty_CurrentDevice,&uid,sizeof(uid));
    if(rc!=noErr){
        printf("stage=set_exact_device result=FAIL code=%d\n",(int)rc);
        AudioQueueDispose(queue,true);CFRelease(uid);return 24;
    }
    CFStringRef actual=NULL;UInt32 n=sizeof(actual);
    rc=AudioQueueGetProperty(queue,kAudioQueueProperty_CurrentDevice,&actual,&n);
    int exact=rc==noErr && actual && CFEqual(uid,actual);
    char actualname[500];readable(actual,actualname,sizeof(actualname));
    printf("stage=route exact=%d selected_uid=%s query_code=%d\n",exact,actualname,(int)rc);
    if(actual)CFRelease(actual);
    if(!exact){AudioQueueDispose(queue,true);CFRelease(uid);return 25;}
    CFRelease(uid);
    double seconds=g_duration;
    state.frames_per_buffer=(uint32_t)(seconds*rate/SEGMENTS);
    UInt32 bufferBytes=state.frames_per_buffer*CHANNELS*sizeof(short);
    for(unsigned i=0;i<SEGMENTS;i++){
        AudioQueueBufferRef buffer=NULL;
        rc=AudioQueueAllocateBuffer(queue,bufferBytes,&buffer);
        if(rc!=noErr || !buffer){printf("stage=allocate result=FAIL code=%d\n",(int)rc);AudioQueueDispose(queue,true);return 26;}
        fill_segment(buffer,i,state.frames_per_buffer,rate);
        rc=AudioQueueEnqueueBuffer(queue,buffer,0,NULL);
        if(rc!=noErr){printf("stage=enqueue result=FAIL code=%d\n",(int)rc);AudioQueueDispose(queue,true);return 27;}
    }
    printf("stage=queued frames=%u seconds=%.2f peak_gain=%.3f sound_origin=Mac_synthesis\n",
           state.frames_per_buffer*SEGMENTS,seconds,g_gain);fflush(stdout);
    rc=AudioQueueStart(queue,NULL);
    if(rc!=noErr){printf("stage=start result=FAIL code=%d\n",(int)rc);AudioQueueDispose(queue,true);return 28;}
    int witnessed_running=0;
    for(int tick=0;tick<(int)(g_duration*10.0)+30;tick++){
        usleep(100000);
        if(device_running(id)==1)witnessed_running=1;
        if(state.returned_buffers>=SEGMENTS)break;
    }
    rc=AudioQueueStop(queue,true);
    printf("stage=audio_test device=%s queued_buffers=%d returned_buffers=%u returned_frames=%llu device_running_observed=%d stop_code=%d "
           "audio_device_port_output=NOT_WITNESSED audible_hearing=NOT_MACHINE_MEASURED\n",
           which,SEGMENTS,state.returned_buffers,(unsigned long long)state.returned_frames,witnessed_running,(int)rc);
    AudioQueueDispose(queue,true);
    return (rc==noErr && state.returned_buffers==SEGMENTS)?0:29;
}
