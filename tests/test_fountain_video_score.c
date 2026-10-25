/* Pure deterministic video score tests. No GPU, HDMI or driver.
 * Run with Sierra clang, macOS clang, or Ubuntu CC.
 */
#include "fountain-video-score.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_basic(void){
    const int w=128,h=72;
    const size_t nr=(size_t)w*h*4,ny=(size_t)w*h*2;
    uint8_t *rgba=(uint8_t*)malloc(nr),*second=(uint8_t*)malloc(nr),
            *uyvy=(uint8_t*)malloc(ny),*uyvy2=(uint8_t*)malloc(ny);
    if(!rgba||!second||!uyvy||!uyvy2)return 1;
    if(fc_video_score_rgba(rgba,nr,w,h,0)!=0)return 2;
    if(fc_video_score_rgba(second,nr,w,h,0)!=0)return 3;
    if(memcmp(rgba,second,nr)!=0)return 4;
    uint32_t crc0=fc_video_score_crc(rgba,nr);
    if(fc_video_score_rgba(second,nr,w,h,91)!=0)return 5;
    uint32_t crc91=fc_video_score_crc(second,nr);
    if(crc0==crc91)return 6;
    if(fc_video_score_uyvy(uyvy,ny,w,h,0)!=0)return 7;
    if(fc_video_score_uyvy(uyvy2,ny,w,h,0)!=0)return 8;
    if(memcmp(uyvy,uyvy2,ny)!=0)return 9;
    if(fc_video_score_uyvy(uyvy2,ny,w,h,91)!=0)return 10;
    if(fc_video_score_crc(uyvy,ny)==fc_video_score_crc(uyvy2,ny))return 11;
    if(fc_video_score_rgba(rgba,nr-1,w,h,0)!=1)return 12;
    if(fc_video_score_uyvy(uyvy,ny-1,w,h,0)!=1)return 13;
    if(fc_video_score_rgba(rgba,nr,w-1,h,0)!=1)return 14;
    if(fc_video_score_rgba(NULL,nr,w,h,0)!=1)return 15;
    /* Distinct cinematic movements must survive both pixel encodings. */
    uint32_t movementCrc[4]={0}, movementYuv[4]={0};
    for(int stage=0;stage<4;stage++){
        uint32_t time=(uint32_t)stage*600u;
        if(fc_video_score_rgba(rgba,nr,w,h,time))return 17;
        if(fc_video_score_uyvy(uyvy,ny,w,h,time))return 18;
        movementCrc[stage]=fc_video_score_crc(rgba,nr);
        movementYuv[stage]=fc_video_score_crc(uyvy,ny);
        for(int earlier=0;earlier<stage;earlier++){
            if(movementCrc[stage]==movementCrc[earlier])return 19;
            if(movementYuv[stage]==movementYuv[earlier])return 20;
        }
    }
    printf("stage=video_score_movements result=PASS"
           " modes=4 rgba=%08x,%08x,%08x,%08x"
           " uyvy=%08x,%08x,%08x,%08x\n",
           (unsigned)movementCrc[0],(unsigned)movementCrc[1],
           (unsigned)movementCrc[2],(unsigned)movementCrc[3],
           (unsigned)movementYuv[0],(unsigned)movementYuv[1],
           (unsigned)movementYuv[2],(unsigned)movementYuv[3]);
    /* Every emitted RGBA sample is opaque by design. */
    for(size_t i=3;i<nr;i+=4)if(rgba[i]!=255)return 16;
    printf("stage=video_score_test result=PASS frames_differ=1"
           " deterministic=1 valid_2vuy=1 rgbo_opaque=1"
           " crc_frame0=0x%08x crc_frame91=0x%08x\n",
           (unsigned)crc0,(unsigned)crc91);
    free(rgba);free(second);free(uyvy);free(uyvy2);
    return 0;
}
int main(void) {
    int rc=test_basic();
    if(rc)printf("stage=video_score_test result=FAIL step=%d\n",rc);
    return rc;
}
