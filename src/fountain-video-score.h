/*
 * Fountain Coach: DIE MASCHINE — deterministic visual score.
 * Single-source creative image kernel for Syphon/OpenGL and HDMI UYVY playback.
 *
 * Pure C99, no OS, GPU, Syphon or vendor SDK dependency. We own these pixels.
 * Video pixel format: UYVY/2vuy, full chroma pair average, limited-range
 * Rec.601 8-bit YUV. Syphon preview format: top-left RGBA8888.
 *
 * Creative structure: two witness-fields, shot / reverse-shot, scan-line
 * seams, wire geometry and a moving red witness signal. Every frame changes.
 */
#ifndef FOUNTAIN_VIDEO_SCORE_H
#define FOUNTAIN_VIDEO_SCORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fc_rgb {
    uint8_t r, g, b;
} fc_rgb;

static inline int fc_abs_i(int a) { return a < 0 ? -a : a; }
static inline int fc_min_i(int a,int b) { return a<b?a:b; }
static inline uint8_t fc_clip_u8(int a) {
    return (uint8_t)(a<0?0:(a>255?255:a));
}

/* A tiny owned 5x7 title alphabet for the video score. No font download. */
static inline uint8_t fc_letter_row(char c,int row) {
    static const uint8_t D[]={30,17,17,17,17,17,30};
    static const uint8_t I[]={31,4,4,4,4,4,31};
    static const uint8_t E[]={31,16,16,30,16,16,31};
    static const uint8_t M[]={17,27,21,21,17,17,17};
    static const uint8_t A[]={14,17,17,31,17,17,17};
    static const uint8_t S[]={15,16,16,14,1,1,30};
    static const uint8_t C[]={15,16,16,16,16,16,15};
    static const uint8_t H[]={17,17,17,31,17,17,17};
    static const uint8_t N[]={17,25,21,19,17,17,17};
    if(row<0||row>=7)return 0;
    switch(c){
      case 'D':return D[row];
      case 'I':return I[row];
      case 'E':return E[row];
      case 'M':return M[row];
      case 'A':return A[row];
      case 'S':return S[row];
      case 'C':return C[row];
      case 'H':return H[row];
      case 'N':return N[row];
      default:return 0;
    }
}

static inline fc_rgb fc_video_score_pixel(int x,int y,int w,int h,uint32_t frame) {
    /* Integer-only renderer. The whole image has a readable visual clock. */
    int phase=(int)(frame%1800u);
    int reverse=(int)((frame/72u)&1u);
    int line=(x*3+y*5+phase*17)%503;
    int grain=(x*29+y*13+phase*7+(x*y)%89)%29;
    int cx=reverse?w*2/3:w/3;
    int cy=h/2;
    int drift=(phase%130)-65;
    cx+=drift*w/1100;
    int dx=(x-cx)*600/w;
    int dy=(y-cy)*600/h;
    int radius2=dx*dx+dy*dy;
    int spoke=(fc_abs_i(dx+dy+(phase%160)-80)<4) ||
              (fc_abs_i(dx-dy-(phase%190)+95)<4);
    int ring=(radius2>=118*118 && radius2<=124*124) ||
             (radius2>=202*202 && radius2<=206*206);
    int seam=(fc_abs_i(x-w/2)<2) || (fc_abs_i(y-h/2)<2);
    int scan=(line<8);
    int border=(x < 8 || x>=w-8 || y<8 || y>=h-8);
    int spotlight=(radius2<110*110 && grain<11);
    int pulse=((phase/4)%41);
    fc_rgb p={(uint8_t)(15+grain/3),(uint8_t)(22+grain/2),(uint8_t)(34+grain)};
    if (seam) {p.r=46;p.g=63;p.b=69;}
    if (scan) {p.r=63;p.g=111;p.b=119;}
    if (ring) {p.r=181;p.g=198;p.b=169;}
    if (spoke && radius2<250*250) {p.r=119;p.g=136;p.b=165;}
    if (spotlight) {p.r=45;p.g=62;p.b=77;}
    if ((x>=w*18/20 && y<h/12) || (y<h/70)) {
        p.r=156;p.g=37+3*pulse;p.b=45;
    }
    if (border) {p.r=126;p.g=42;p.b=60;}
    /* Distinct lower-left witness ID pulses that encode frame progression. */
    if (y>h*91/100 && y<h*97/100 && x>=w/18 && x<w*9/18) {
        int unit=fc_min_i(9,(x-w/18)*10/(w*8/18+1));
        if (((frame/3u)>>unit)&1u) {p.r=205;p.g=173;p.b=84;}
    }
    /* Four cinematic movements, alternating every 600 output frame counts:
       an articulated eye, an amber shot/reverse, a silver negative and a
       drawn title. The Syphon preview and HDMI share these exact pixels. */
    const int movement=(int)((frame/600u)%4u);
    if(movement==1){
        int stripe=(x*19+y*3+phase*23)%311;
        int copper=(((x/31)+(y/57)+(int)(frame/24u))%13)<5;
        p.r=(uint8_t)(copper?151+grain:34+grain);
        p.g=(uint8_t)(copper?93+grain:88+grain);
        p.b=(uint8_t)(copper?46:113+grain);
        if(stripe<9){p.r=228;p.g=190;p.b=127;}
        if(radius2>181*181&&radius2<193*193){
            p.r=245;p.g=225;p.b=188;
        }
        if(y>h/5 && y<h*4/5 && fc_abs_i(x-w/2)<5){
            p.r=248;p.g=229;p.b=182;
        }
    } else if(movement==2){
        int pillar=(x+(int)(frame%300u)*5)%(w/3+1);
        int window=(pillar>w/15 && pillar<w/5 &&
                    y>h/7 && y<h*6/7);
        p.r=(uint8_t)(204-grain*2);
        p.g=(uint8_t)(214-grain);
        p.b=(uint8_t)(196-grain);
        if(window){p.r=37;p.g=64;p.b=81;}
        if(((x*3+y*11+phase*7)%239)<6){
            p.r=54;p.g=79;p.b=88;
        }
        if(radius2<120*120){p.r=189;p.g=42;p.b=54;}
        if(fc_abs_i(x-w/2)<4){p.r=26;p.g=43;p.b=57;}
    } else if(movement==3){
        int band=(((x*5+y*7+(int)(frame%240u)*11)%401)<12);
        p.r=(uint8_t)(11+grain/3);
        p.g=(uint8_t)(19+grain/2);
        p.b=(uint8_t)(29+grain);
        if(band){p.r=45;p.g=90;p.b=100;}
        if(y>h*9/10){p.r=150;p.g=41;p.b=55;}
        const char* title="DIE MASCHINE";
        const int scale=w/160>1?w/160:2;
        const int title_width=12*6*scale;
        const int x0=(w-title_width)/2;
        const int y0=h/2-4*scale;
        if(x>=x0 && x<x0+title_width &&
           y>=y0 && y<y0+7*scale){
            const int lx=(x-x0)/scale,ly=(y-y0)/scale;
            const int character=lx/6, column=lx%6;
            if(column<5 &&
               (fc_letter_row(title[character],ly)&(1u<<(4-column)))){
                p.r=238;p.g=227;p.b=191;
            }
        }
    }
    return p;
}

static inline void fc_rgb_to_yuv601(fc_rgb p,int* y,int* u,int* v) {
    *y=16+((66*(int)p.r+129*(int)p.g+25*(int)p.b+128)>>8);
    *u=128+((-38*(int)p.r-74*(int)p.g+112*(int)p.b+128)>>8);
    *v=128+((112*(int)p.r-94*(int)p.g-18*(int)p.b+128)>>8);
}

static inline int fc_video_score_valid(int w,int h) {
    return w>=64 && h>=64 && w<=4096 && h<=2160 && (w%2)==0;
}

/* Data length must be >= w*h*2. Returns 0 on success. */
static inline int fc_video_score_uyvy(uint8_t* output,size_t capacity,
                                       int w,int h,uint32_t frame) {
    if(!output||!fc_video_score_valid(w,h)||capacity<(size_t)w*(size_t)h*2u)
        return 1;
    for(int y=0;y<h;y++) {
        for(int x=0;x<w;x+=2) {
            fc_rgb a=fc_video_score_pixel(x,y,w,h,frame);
            fc_rgb b=fc_video_score_pixel(x+1,y,w,h,frame);
            int y0,u0,v0,y1,u1,v1;
            fc_rgb_to_yuv601(a,&y0,&u0,&v0);
            fc_rgb_to_yuv601(b,&y1,&u1,&v1);
            size_t p=((size_t)y*(size_t)w+(size_t)x)*2u;
            output[p+0]=fc_clip_u8((u0+u1+1)/2);
            output[p+1]=fc_clip_u8(y0);
            output[p+2]=fc_clip_u8((v0+v1+1)/2);
            output[p+3]=fc_clip_u8(y1);
        }
    }
    return 0;
}

/* Data length must be >= w*h*4. Returns 0 on success. */
static inline int fc_video_score_rgba(uint8_t* output,size_t capacity,
                                      int w,int h,uint32_t frame) {
    if(!output||!fc_video_score_valid(w,h)||capacity<(size_t)w*(size_t)h*4u)
        return 1;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        fc_rgb c=fc_video_score_pixel(x,y,w,h,frame);
        size_t p=((size_t)y*(size_t)w+(size_t)x)*4u;
        output[p+0]=c.r;
        output[p+1]=c.g;
        output[p+2]=c.b;
        output[p+3]=255;
    }
    return 0;
}

static inline uint32_t fc_video_score_crc(const uint8_t* p,size_t n){
    /* FNV-1a: fast reproducible fingerprint, not a crypto signature. */
    uint32_t h=2166136261u;
    for(size_t i=0;i<n;i++) h=(h^p[i])*16777619u;
    return h;
}
#ifdef __cplusplus
}
#endif
#endif
