/*
 * Source-owned, synthetic IDeckLinkVideoInputFrame ABI fixture.
 * Compiles the native observer's callback but does not run its main or
 * load DeckLinkAPI, nor touch the Blackmagic hardware.
 */
#define main fc_decklink_observer_main
#include "../src/decklink-input-observer.cpp"
#undef main
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct MockFrame {
    void **vtable;
    uint32_t flags;
    int bytesError;
    int timeError;
    uint8_t buffer[16];
};
static long mockWidth(void*) { return 4; }
static long mockHeight(void*) { return 2; }
static long mockRowBytes(void*) { return 8; }
static uint32_t mockFlags(void* p) { return ((MockFrame*)p)->flags; }
static int32_t mockGetBytes(void* p, void** out) {
    MockFrame* f=(MockFrame*)p;
    if (f->bytesError) { *out=nullptr; return (int32_t)0x80004005; }
    *out=f->buffer; return 0;
}
static int32_t mockStreamTime(void* p, int64_t* start, int64_t* duration,
                              int64_t scale) {
    MockFrame* f=(MockFrame*)p;
    if (f->timeError || scale != 30000) return (int32_t)0x80004005;
    *start=120000; *duration=1000; return 0;
}
static void* mockVtable[13] = {};

static void reset(Observer* s) {
    s->vtable=callbackTable;
    s->refs.store(1);
    s->frames.store(0); s->valid.store(0); s->noSource.store(0);
    s->formatChanges.store(0);
    s->firstFrameBytesOk.store(0); s->firstFrameWidth.store(0);
    s->firstFrameHeight.store(0); s->firstFrameRowBytes.store(0);
    s->firstFrameSampleCount.store(0); s->firstFrameSampleHash.store(0);
    s->firstFrameTimeOk.store(0); s->firstFrameTime.store(0);
    s->firstFrameDuration.store(0);
}
int main(void) {
    mockVtable[3]=(void*)mockWidth;
    mockVtable[4]=(void*)mockHeight;
    mockVtable[5]=(void*)mockRowBytes;
    mockVtable[7]=(void*)mockFlags;
    mockVtable[8]=(void*)mockGetBytes;
    mockVtable[11]=(void*)mockStreamTime;

    Observer observer;
    reset(&observer);
    MockFrame f = {mockVtable, 0x80000000u, 0, 0, {0}};
    for(unsigned i=0; i<16; ++i) f.buffer[i]=(uint8_t)(i*3u);
    assert(onFrame(&observer,&f,nullptr)==0);
    assert(observer.frames.load()==1 && observer.noSource.load()==1);
    assert(observer.valid.load()==0 && observer.firstFrameBytesOk.load()==0);

    reset(&observer);
    f.flags=0;
    assert(onFrame(&observer,&f,nullptr)==0);
    assert(observer.valid.load()==1 && observer.noSource.load()==0);
    assert(observer.firstFrameBytesOk.load()==1);
    assert(observer.firstFrameSampleCount.load()==16);
    assert(observer.firstFrameWidth.load()==4 && observer.firstFrameHeight.load()==2);
    assert(observer.firstFrameRowBytes.load()==8);
    uint32_t h=2166136261u;
    for(unsigned i=0; i<16; ++i) {
        h^=f.buffer[i]; h*=16777619u;
    }
    assert(observer.firstFrameSampleHash.load()==h);
    assert(observer.firstFrameTimeOk.load()==1);
    assert(observer.firstFrameTime.load()==120000);
    assert(observer.firstFrameDuration.load()==1000);
    const uint32_t firstHash=observer.firstFrameSampleHash.load();
    f.buffer[0]=99;
    assert(onFrame(&observer,&f,nullptr)==0);
    assert(observer.valid.load()==2);
    assert(observer.firstFrameSampleHash.load()==firstHash);

    reset(&observer);
    f.bytesError=1;
    assert(onFrame(&observer,&f,nullptr)==0);
    assert(observer.valid.load()==1 && observer.firstFrameBytesOk.load()==0);
    assert(observer.firstFrameTimeOk.load()==1);

    reset(&observer);
    f.bytesError=0; f.timeError=1;
    assert(onFrame(&observer,&f,nullptr)==0);
    assert(observer.firstFrameBytesOk.load()==1);
    assert(observer.firstFrameTimeOk.load()==0);

    printf("stage=observer_first_frame_fixture result=pass cases=5 hardware_access=0\n");
    return 0;
}
