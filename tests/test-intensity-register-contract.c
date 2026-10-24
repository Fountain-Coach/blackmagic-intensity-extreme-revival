#include "../src/intensity-register-contract.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct Fixture {
    uint32_t version;
    uint32_t status;
    int failure_offset;
    uint32_t reads;
    uint32_t seen_offsets[2];
};

static int mock_read32(void* arg, uint32_t offset, uint32_t* out) {
    struct Fixture* f = (struct Fixture*)arg;
    assert(f->reads < 2);
    f->seen_offsets[f->reads++] = offset;
    if (f->failure_offset == (int)offset) return 0;
    if (offset == FC_A124_VERSION_BAR0_OFFSET) *out = f->version;
    else if (offset == FC_A124_INPUT_STATUS_BAR0_OFFSET) *out = f->status;
    else return 0;
    return 1;
}

static FCFirmwareGateContext fwg(void) {
    FCFirmwareGateContext g = {0, 0, 0};
    return g;
}

static FCInputStatusGates ing(void) {
    FCInputStatusGates g = {0, 0, 0, 8};
    return g;
}

int main(void) {
    struct Fixture f={0x1234000b,0x1,-1,0,{0,0}};
    FCReadContractResult r=fc_evaluate_a124_read_contract(
        mock_read32,&f,fwg(),ing());
    assert(r.state == FC_READ_CONTRACT_STATUS_INTERPRETED);
    assert(r.firmware == FC_FIRMWARE_VERSION_ACCEPTED);
    assert(r.reported_version == 0x0b && r.register_reads == 2);
    assert(f.reads == 2 && f.seen_offsets[0] == 0x224 &&
           f.seen_offsets[1] == 0x40);
    assert(r.input.classification == FC_INPUT_BASIC_FORMAT_CANDIDATE);

    f=(struct Fixture){0x0000000a,0x1,-1,0,{0,0}};
    r=fc_evaluate_a124_read_contract(mock_read32,&f,fwg(),ing());
    assert(r.state==FC_READ_CONTRACT_VERSION_REJECTED && f.reads==1);

    f=(struct Fixture){UINT32_MAX,0x1,-1,0,{0,0}};
    r=fc_evaluate_a124_read_contract(mock_read32,&f,fwg(),ing());
    assert(r.state==FC_READ_CONTRACT_VERSION_UNREADABLE && f.reads==1);

    f=(struct Fixture){0x0b,0x1,FC_A124_VERSION_BAR0_OFFSET,0,{0,0}};
    r=fc_evaluate_a124_read_contract(mock_read32,&f,fwg(),ing());
    assert(r.state==FC_READ_CONTRACT_VERSION_UNREADABLE && f.reads==1);

    f=(struct Fixture){0x0b,0x0,FC_A124_INPUT_STATUS_BAR0_OFFSET,0,{0,0}};
    r=fc_evaluate_a124_read_contract(mock_read32,&f,fwg(),ing());
    assert(r.state==FC_READ_CONTRACT_STATUS_UNREADABLE && f.reads==2);

    f=(struct Fixture){0x0b,UINT32_MAX,-1,0,{0,0}};
    r=fc_evaluate_a124_read_contract(mock_read32,&f,fwg(),ing());
    assert(r.state==FC_READ_CONTRACT_STATUS_UNREADABLE && f.reads==2);

    f=(struct Fixture){0x0b,0x0,-1,0,{0,0}};
    r=fc_evaluate_a124_read_contract(mock_read32,&f,fwg(),ing());
    assert(r.state==FC_READ_CONTRACT_STATUS_INTERPRETED && f.reads==2);
    assert(r.input.classification==FC_INPUT_NO_SIGNAL_FLAG);

    FCFirmwareGateContext fg=fwg();
    fg.guard_289=1;
    f=(struct Fixture){0x0b,0x1,-1,0,{0,0}};
    r=fc_evaluate_a124_read_contract(mock_read32,&f,fg,ing());
    assert(r.state==FC_READ_CONTRACT_GUARDS_CLOSED && f.reads==0);

    fg=fwg(); fg.skip_version_check_28c=1;
    f=(struct Fixture){0,0x1,-1,0,{0,0}};
    r=fc_evaluate_a124_read_contract(mock_read32,&f,fg,ing());
    assert(r.state==FC_READ_CONTRACT_STATUS_INTERPRETED && f.reads==1);
    assert(f.seen_offsets[0]==0x40);
    assert(r.firmware==FC_FIRMWARE_COMPARISON_SKIPPED);

    f=(struct Fixture){0x0b,0x1,-1,0,{0,0}};
    r=fc_evaluate_a124_read_contract(NULL,&f,fwg(),ing());
    assert(r.state==FC_READ_CONTRACT_INVALID && f.reads==0);

    printf("stage=read_contract result=pass cases=10 hardware_access=0\n");
    return 0;
}
