/*
 * Sierra-native mock of the original DeckLink IDeckLinkVideoInputFrame ABI.
 *
 * Reuses the actual source-owned observer's callback and vtable code by
 * compiling the source into this isolated executable with a renamed main.
 * Neither its real main nor the installed Desktop Video framework is called.
 *
 * Synthetic frames validate: no-input-source flag, width/height/stride,
 * GetBytes, sample FNV32, 30k timebase, query/release refcounts, format event,
 * and that callbacks do not retain/release borrowed video-frame objects.
 *
 * Pass != a working Blackmagic driver or a witnessed HDMI input frame.
 */
#define main fc_original_decklink_observer_main_NOT_CALLED
#include "../src/decklink-input-observer.cpp"
#undef main

#include <cassert>
#include <stdint.h>

struct FakeFrame {
    void **vtable;
    uint32_t flags;
    long width;
    long height;
    long stride;
    int64_t time;
    int64_t duration;
    uint32_t references;
    uint32_t addref_calls;
    uint32_t release_calls;
    unsigned char image[16];
};

static uint32_t fake_addref(void *p) {
    FakeFrame *self=(FakeFrame *)p;
    ++self->addref_calls;
    return ++self->references;
}
static uint32_t fake_release(void *p) {
    FakeFrame *self=(FakeFrame *)p;
    ++self->release_calls;
    return --self->references;
}
static long fake_width(void *p) { return ((FakeFrame *)p)->width; }
static long fake_height(void *p) { return ((FakeFrame *)p)->height; }
static long fake_stride(void *p) { return ((FakeFrame *)p)->stride; }
static uint32_t fake_flags(void *p) {return ((FakeFrame *)p)->flags;}
static int32_t fake_bytes(void *p, void **out) {
    if(!out) return (int32_t)0x80004003;
    *out=((FakeFrame *)p)->image;
    return 0;
}
static int32_t fake_time(void *p, int64_t *t, int64_t *d, int64_t scale) {
    if(scale!=30000 || !t || !d) return (int32_t)0x80004005;
    *t=((FakeFrame *)p)->time;
    *d=((FakeFrame *)p)->duration;
    return 0;
}
static void *fakeVtable[13];

static void initialize_observer(Observer &state) {
    state.vtable=callbackTable;
    state.refs.store(1);
    state.frames.store(0);
    state.valid.store(0);
    state.noSource.store(0);
    state.formatChanges.store(0);
    state.firstFrameBytesOk.store(0);
    state.firstFrameWidth.store(0);
    state.firstFrameHeight.store(0);
    state.firstFrameRowBytes.store(0);
    state.firstFrameSampleCount.store(0);
    state.firstFrameSampleHash.store(0);
    state.firstFrameTimeOk.store(0);
    state.firstFrameTime.store(0);
    state.firstFrameDuration.store(0);
}
static uint32_t fnv32(const unsigned char *pixels, size_t count) {
    uint32_t hash=2166136261u;
    for (size_t i=0;i<count;++i) {
        hash^=(uint32_t)pixels[i];
        hash*=16777619u;
    }
    return hash;
}
static FakeFrame make_frame(uint32_t flags, long stride) {
    FakeFrame result={};
    result.vtable=fakeVtable;
    result.flags=flags;
    result.width=4;
    result.height=2;
    result.stride=stride;
    result.time=1023;
    result.duration=1000;
    result.references=1;
    const unsigned char example[16]={
        128,235,128,16,128,16,128,235,
        128,235,128,16,128,16,128,235
    };
    memcpy(result.image,example,sizeof(example));
    return result;
}
int main(void) {
    // The original observer's vtable and public interface calls are tested
    // as compiled, against a source-owned fake provider.
    fakeVtable[1]=(void*)fake_addref;
    fakeVtable[2]=(void*)fake_release;
    fakeVtable[3]=(void*)fake_width;
    fakeVtable[4]=(void*)fake_height;
    fakeVtable[5]=(void*)fake_stride;
    fakeVtable[7]=(void*)fake_flags;
    fakeVtable[8]=(void*)fake_bytes;
    fakeVtable[11]=(void*)fake_time;

    Observer s;
    initialize_observer(s);
    void *out=nullptr;
    CFUUIDBytes iid;
    memcpy(&iid,callbackIID,sizeof(iid));
    assert(queryObserver(&s,iid,&out)==0 && out==&s);
    assert(s.refs.load()==2);
    assert(releaseRef(&s)==1);
    memcpy(&iid,unknownIID,sizeof(iid));
    assert(queryObserver(&s,iid,&out)==0 && out==&s);
    assert(releaseRef(&s)==1);
    memset(&iid,0xef,sizeof(iid));
    out=(void*)0x2;
    assert(queryObserver(&s,iid,&out)!=(int32_t)0 && out==nullptr);

    FakeFrame absent=make_frame(0x80000000u,8);
    assert(onFrame(&s,&absent,nullptr)==0);
    assert(s.frames.load()==1 && s.noSource.load()==1);
    assert(s.valid.load()==0 && absent.references==1);
    assert(absent.addref_calls==0 && absent.release_calls==0);

    FakeFrame input=make_frame(0,8);
    assert(onFormatChange(&s,0,nullptr,0)==0);
    assert(onFrame(&s,&input,nullptr)==0);
    assert(s.frames.load()==2 && s.valid.load()==1);
    assert(s.formatChanges.load()==1);
    assert(s.firstFrameBytesOk.load()==1);
    assert(s.firstFrameWidth.load()==4);
    assert(s.firstFrameHeight.load()==2);
    assert(s.firstFrameRowBytes.load()==8);
    assert(s.firstFrameSampleCount.load()==16);
    assert(s.firstFrameSampleHash.load()==fnv32(input.image,sizeof(input.image)));
    assert(s.firstFrameTimeOk.load()==1);
    assert(s.firstFrameTime.load()==1023);
    assert(s.firstFrameDuration.load()==1000);
    // The client correctly copies/accesses a BORROWED frame; it must never
    // change the vendor's frame refcount inside VideoInputFrameArrived.
    assert(input.references==1);
    assert(input.addref_calls==0 && input.release_calls==0);

    assert(onFrame(&s,nullptr,nullptr)==0 && s.frames.load()==2);
    FakeFrame second=make_frame(0,8);
    assert(onFrame(&s,&second,nullptr)==0 && s.valid.load()==2);
    assert(second.references==1 && second.release_calls==0);
    assert(s.refs.load()==1);

    // The observer is valid under its own documented callback semantics,
    // but this cannot diagnose corrupted CDeckLinkVideoInputFrame::member+B0.
    printf("stage=decklink_callback_mock result=pass"
           " tests=12 queried=1 no_source=1 valid_video=2"
           " sample_fnv32=0x%08x timebase=30000 borrowed_release_calls=0"
           " hardware_access=0 vendor_library_loaded=0\n",
           s.firstFrameSampleHash.load());
    return 0;
}
