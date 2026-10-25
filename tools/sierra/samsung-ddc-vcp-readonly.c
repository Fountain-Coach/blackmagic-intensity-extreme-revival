/*
 * Fountain Coach / Sierra — read-only Samsung HDMI input selector witness.
 *
 * Query MCCS VCP 0x60 on the monitor identified by the original EDID.
 * This is DDC/CI feature GET, not an input selection WRITE.
 *
 * Caution: Sierra Intel framebuffer I2C is an OS driver, not our own
 * device driver. One bounded synchronous request, no retries, no resets,
 * no DPMS, no SetVCPFeature, no change to Blackmagic 10.4.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/i2c/IOI2CInterface.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int metadata_matches(io_service_t fb){
    CFDictionaryRef dict=IODisplayCreateInfoDictionary(
        fb,kIODisplayOnlyPreferredName);
    if(!dict)return 0;
    const char *keys[]={kDisplayVendorID,kDisplayProductID,kDisplaySerialNumber};
    const long target[]={19501,3148,810375238};
    int result=1;
    for(int i=0;i<3;i++){
        CFNumberRef number=(CFNumberRef)CFDictionaryGetValue(
            dict,CFStringCreateWithCStringNoCopy(kCFAllocatorDefault,keys[i],
               kCFStringEncodingUTF8,kCFAllocatorNull));
        long v=-1;
        if(!number || CFGetTypeID(number)!=CFNumberGetTypeID() ||
           !CFNumberGetValue(number,kCFNumberLongType,&v) || v!=target[i]){
            result=0;break;
        }
    }
    CFRelease(dict);
    return result;
}
int main(void){
    const CGDirectDisplayID main=CGMainDisplayID();
    if(CGDisplayIsBuiltin(main) ||
       CGDisplayVendorNumber(main)!=19501 ||
       CGDisplayModelNumber(main)!=3148 ||
       CGDisplaySerialNumber(main)!=810375238){
        puts("stage=samsung_ddc result=WRONG_MONITOR_NO_REQUEST");return 20;
    }
    io_iterator_t it=0;
    kern_return_t kr=IOServiceGetMatchingServices(
        kIOMasterPortDefault,IOServiceMatching("IOFramebuffer"),&it);
    if(kr!=KERN_SUCCESS){
        printf("stage=samsung_ddc result=NO_IOFRAMEBUFFER err=0x%x\n",(unsigned)kr);
        return 21;
    }
    io_service_t framebuffer=0,current;
    while((current=IOIteratorNext(it))!=0){
        if(metadata_matches(current)){
            if(framebuffer){
                IOObjectRelease(current);
                IOObjectRelease(framebuffer);
                IOObjectRelease(it);
                puts("stage=samsung_ddc result=AMBIGUOUS_MONITOR_NO_REQUEST");
                return 22;
            }
            framebuffer=current;
        }else IOObjectRelease(current);
    }
    IOObjectRelease(it);
    if(!framebuffer){
        puts("stage=samsung_ddc result=NO_MATCHED_FRAMEBUFFER");return 23;
    }
    io_name_t name;memset(name,0,sizeof(name));
    IORegistryEntryGetName(framebuffer,name);
    printf("stage=samsung_ddc framebuffer=%s exact_EDID_match=1\n",name);
    IOItemCount buses=0;
    kr=IOFBGetI2CInterfaceCount(framebuffer,&buses);
    printf("stage=samsung_ddc bus_count_rc=0x%x bus_count=%u\n",
           (unsigned)kr,(unsigned)buses);
    if(kr!=KERN_SUCCESS||buses!=1){
        IOObjectRelease(framebuffer);return 24;
    }
    io_service_t interface=0;
    kr=IOFBCopyI2CInterfaceForBus(framebuffer,0,&interface);
    IOObjectRelease(framebuffer);
    if(kr!=KERN_SUCCESS||!interface){
        puts("stage=samsung_ddc result=NO_I2C_INTERFACE");return 25;
    }
    IOI2CConnectRef conn=0;
    kr=IOI2CInterfaceOpen(interface,kNilOptions,&conn);
    IOObjectRelease(interface);
    if(kr!=KERN_SUCCESS){
        printf("stage=samsung_ddc result=OPEN_REFUSED rc=0x%x\n",(unsigned)kr);
        return 26;
    }
    uint8_t request_bytes[5]={0x51,0x82,0x01,0x60,0};
    request_bytes[4]=0x6e^request_bytes[0]^request_bytes[1]^
                     request_bytes[2]^request_bytes[3];
    uint8_t reply[11]={0};
    IOI2CRequest query;memset(&query,0,sizeof(query));
    query.sendAddress=0x6e;
    query.sendTransactionType=kIOI2CSimpleTransactionType;
    query.sendBuffer=(vm_address_t)request_bytes;
    query.sendBytes=5;
    query.minReplyDelay=50000000u; /* bounded 50 ms quiet delay */
    query.replyAddress=0x6f;
    query.replySubAddress=0x51;
    query.replyTransactionType=kIOI2CSimpleTransactionType;
    query.replyBuffer=(vm_address_t)reply;
    query.replyBytes=sizeof(reply);
    printf("stage=samsung_ddc request=GET_VCP_0x60"
           " write_setting=NO retry=NO min_reply_delay_ns=50000000\n");
    fflush(stdout);
    kr=IOI2CSendRequest(conn,kNilOptions,&query);
    kern_return_t close_rc=IOI2CInterfaceClose(conn,kNilOptions);
    printf("stage=samsung_ddc request_rc=0x%x bus_rc=0x%x"
           " bytes_received=%u close_rc=0x%x\n",
           (unsigned)kr,(unsigned)query.result,
           (unsigned)query.replyBytes,(unsigned)close_rc);
    if(kr!=KERN_SUCCESS || query.result!=KERN_SUCCESS ||
       query.replyBytes<11){
        puts("stage=samsung_ddc result=NO_VERIFIED_MONITOR_RESPONSE");
        return 27;
    }
    uint8_t sum=0x6f^0x51;
    for(size_t i=0;i<10;i++)sum^=reply[i];
    int correct=reply[0]==0x6e && reply[2]==0x02 &&
        reply[4]==0x60 && reply[10]==sum && reply[3]==0;
    if(!correct){
        printf("stage=samsung_ddc result=INVALID_VCP_REPLY"
               " checksum_expected=0x%02x checksum_actual=0x%02x\n",
               (unsigned)sum,(unsigned)reply[10]);
        return 28;
    }
    uint16_t max=(uint16_t)(reply[6]<<8 |reply[7]);
    uint16_t current_source=(uint16_t)(reply[8]<<8 |reply[9]);
    printf("stage=samsung_ddc result=VERIFIED_INPUT_VCP"
           " current=%u max=%u source_type=%s"
           " physical_video_pixels=UNVERIFIED\n",
           (unsigned)current_source,(unsigned)max,
           current_source==17?"HDMI_1":
           current_source==18?"HDMI_2":
           current_source==15?"DisplayPort_1":
           current_source==16?"DisplayPort_2":"OTHER_OR_UNKNOWN");
    return 0;
}
