/*
 * Fountain Coach original Intensity Extreme HDMI OUTPUT sink status.
 * Read-only, non-mutating. Probe IDeckLinkStatus available on later SDKs.
 *
 * Original Sierra Desktop Video 10.4 may predate this interface. In that
 * case report NOT_EXPOSED, NOT "no Panasonic" or "no output signal".
 *
 * If the interface is present, request bmdDeckLinkStatusReceivedEDID.
 * An EDID proves sink handshaking data, NOT successful image pixels.
 *
 * No vendor headers/binaries included; no StartStreams / output enable /
 * configuration writes / PCI MMIO / driver changes.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <sys/sysctl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef void* (*Factory)(void);
typedef int32_t (*Query)(void*,CFUUIDBytes,void**);
typedef int32_t (*Next)(void*,void**);
typedef int32_t (*GetBytes)(void*,uint32_t,void*,uint32_t*);
typedef int32_t (*GetInt)(void*,uint32_t,int64_t*);
typedef uint32_t (*Release)(void*);
static void release_ref(void* p){if(p)((Release)(*(void***)p)[2])(p);}
static int source_machine(void){
 char model[64]={0},kernel[64]={0};size_t ml=sizeof(model),kl=sizeof(kernel);
 return !sysctlbyname("hw.model",model,&ml,NULL,0) &&
        !sysctlbyname("kern.osrelease",kernel,&kl,NULL,0) &&
        !strcmp(model,"Macmini5,3") && !strncmp(kernel,"16.",3);
}
static const uint8_t kStatusIID[]={
 0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,
 0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46
};
static int edid_valid(const uint8_t* p,size_t n){
 static const uint8_t header[]={0,255,255,255,255,255,255,0};
 if(n<128||n%128||n>1024||memcmp(p,header,8))return 0;
 for(size_t at=0;at<n;at+=128){
  int sum=0;
  for(size_t j=0;j<128;j++)sum+=p[at+j];
  if(sum%256)return 0;
 }
 return 1;
}
int main(void){
 if(!source_machine()){
   puts("stage=hdmi_sink_status result=BLOCKED_WRONG_ORIGINAL_HOST");
   return 20;
 }
 void* library=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                      RTLD_NOW|RTLD_LOCAL);
 if(!library){puts("stage=hdmi_sink_status result=NO_LEGACY_FRAMEWORK");return 21;}
 Factory make=(Factory)dlsym(library,"CreateDeckLinkIteratorInstance");
 void* it=make?make():NULL;
 if(!it){dlclose(library);return 22;}
 void* device=NULL;
 int32_t rc=((Next)(*(void***)it)[3])(it,&device);
 release_ref(it);
 if(rc||!device){dlclose(library);return 23;}
 CFUUIDBytes id;memcpy(&id,kStatusIID,16);
 void* status=NULL;
 rc=((Query)(*(void***)device)[0])(device,id,&status);
 printf("stage=hdmi_sink_status query_status_interface=0x%08x available=%d\n",
        (unsigned)rc,status!=NULL);
 fflush(stdout);
 if(rc||!status){
   puts("stage=hdmi_sink_status result=NOT_EXPOSED_BY_ORIGINAL_RUNTIME"
        " panasonic_connected=UNKNOWN physical_output_locked=UNKNOWN");
   release_ref(device);
   dlclose(library);
   return 0;
 }
 int64_t mode=0;
 rc=((GetInt)(*(void***)status)[4])(status,0x63766f6d,&mode);
 printf("stage=hdmi_sink_status output_mode_status=0x%08x output_mode=0x%08x\n",
        (unsigned)rc,(unsigned)(uint32_t)mode);
 /* 10.4 status returns E_OUTOFMEMORY and required size 4096 for a
    1024-byte buffer. Ask the runtime for its required capacity without
    assuming the number of valid EDID blocks equals that capacity. */
 uint32_t required=0;
 int32_t size_rc=((GetBytes)(*(void***)status)[7])(
     status,0x65646964u,NULL,&required);
 printf("stage=hdmi_sink_status edid_size_query_rc=0x%08x required=%u\n",
        (unsigned)size_rc,(unsigned)required);
 uint8_t *edid=NULL;
 if(required>=128 && required<=65536){
    edid=(uint8_t*)calloc((size_t)required,1u);
 }
 if(!edid){
    printf("stage=hdmi_sink_status result=NO_SAFE_EDID_BUFFER"
           " required=%u panasonic_presence=UNKNOWN\n",(unsigned)required);
 }else{
    uint32_t returned=required;
    rc=((GetBytes)(*(void***)status)[7])(
       status,0x65646964u,edid,&returned);
    printf("stage=hdmi_sink_status edid_read_rc=0x%08x"
           " capacity=%u returned=%u\n",
           (unsigned)rc,(unsigned)required,(unsigned)returned);
    size_t actualBlocks=0u;
    if(!rc && returned>=128u &&
       !memcmp(edid,"\x00\xff\xff\xff\xff\xff\xff\x00",8)){
      actualBlocks=(size_t)edid[126]+1u;
      if(actualBlocks>(size_t)returned/128u || actualBlocks>8u)
         actualBlocks=0u;
    }
    const size_t validLength=actualBlocks*128u;
    if(!rc && validLength && edid_valid(edid,validLength)){
        unsigned vendor=((unsigned)edid[8]<<8)|(unsigned)edid[9];
        unsigned product=(unsigned)edid[10]|((unsigned)edid[11]<<8);
        char name[4]={0};
        name[0]=(char)('A'+((vendor>>10)&31)-1);
        name[1]=(char)('A'+((vendor>>5)&31)-1);
        name[2]=(char)('A'+(vendor&31)-1);
        printf("stage=hdmi_sink_status result=RECEIVED_VALID_SINK_EDID"
               " manufacturer=%s product=0x%04x extensions=%u"
               " valid_edid_bytes=%lu physical_pixels=UNVERIFIED\n",
               name,product,(unsigned)edid[126],(unsigned long)validLength);
        for(size_t desc=54u;desc+18u<=126u;desc+=18u){
            if(edid[desc] || edid[desc+1] || edid[desc+2])continue;
            if(edid[desc+3]==0xfcu){
               char display[14]={0};
               for(size_t i=0;i<13u;i++){
                   const unsigned char c=edid[desc+5u+i];
                   if(c=='\n'||c=='\r'||c==0)break;
                   display[i]=(c>=32&&c<127)?(char)c:'?';
               }
               printf("stage=hdmi_sink_status edid_display_name=%s\n",display);
            }
        }
        for(size_t ext=1;ext<actualBlocks;ext++){
           const uint8_t* p=edid+ext*128u;
           if(p[0]!=0x02u)continue;
           size_t end=(size_t)p[2];
           if(end<4u||end>127u)continue;
           printf("stage=hdmi_sink_status cta_revision=%u"
                  " supports_422=%u supports_444=%u\n",
                  (unsigned)p[1],(unsigned)((p[3]>>4)&1),
                  (unsigned)((p[3]>>5)&1));
           size_t at=4u;
           while(at<end){
             unsigned tag=p[at]>>5,n=p[at]&31u;
             if(at+1u+(size_t)n>end)break;
             if(tag==2){
                 for(unsigned j=0;j<n;j++){
                    printf("stage=hdmi_sink_status cta_video_vic=%u native=%u\n",
                           (unsigned)(p[at+1u+j]&127u),
                           (unsigned)(p[at+1u+j]>>7));
                 }
             }
             at+=1u+(size_t)n;
           }
        }
    }else{
        printf("stage=hdmi_sink_status result=NO_VALID_EDID_READ"
               " rc=0x%08x returned=%u"
               " first_eight=", (unsigned)rc,(unsigned)returned);
        for(size_t j=0;j<8u && j<(size_t)required;j++)
            printf("%02x",(unsigned)edid[j]);
        puts(" panasonic_presence=UNKNOWN");
    }
    free(edid);
 }
 puts("stage=hdmi_sink_status no_hardware_changes=1"
      " vendor_capture_calls=0 vendor_output_calls=0"
      " electrical_signal_measured=0");
 release_ref(status);
 release_ref(device);
 dlclose(library);
 return 0;
}
