
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "debug.h"
#include "sdbuf.h"

#define BUF_SIZE (2048)

typedef struct test_value_t {
    const char *name;
    sdbtypes_t type;
    sdb_val_t  value;
} test_value_t;

test_value_t test_values[] = {
    { .name = "james",  .type = SDB_S16, .value.s16 = 0x55aa, },
    { .name = "ido",    .type = SDB_U32, .value.u32 = 0x12345678, },
    { .name = "bud",    .type = SDB_S16, .value.s16 = 0xf00d, },
    { .name = "jeremy", .type = SDB_U32, .value.u32 = 0xdeadbeef, },
    { .name = "lewis",  .type = SDB_U64, .value.u64 = 0xaabbccdd99887766ULL, },
    { .name = "andrew", .type = SDB_S16, .value.s16 = 0xd00f, },
    { .name = "david",  .type = SDB_S8,  .value.s8 = -12, },
};

int8_t check_ok(const sdb_val_t a, const sdb_val_t b, const sdbtypes_t t) {
    int8_t rv = 0;
    switch (t) {
        case SDB_S8:  if (a.s8  != b.s8)  rv = -1; break;
        case SDB_U8:  if (a.u8  != b.u8)  rv = -2; break;
        case SDB_S16: if (a.s16 != b.s16) rv = -3; break;
        case SDB_U16: if (a.u16 != b.u16) rv = -4; break;
        case SDB_S32: if (a.s32 != b.s32) rv = -5; break;
        case SDB_U32: if (a.u32 != b.u32) rv = -6; break;
        case SDB_S64: if (a.s64 != b.s64) rv = -7; break;
        case SDB_U64: if (a.u64 != b.u64) rv = -8; break;
        default: break;
    }
    return rv;
}


int test_one() {

    sdb_t sdb;
    uint8_t buf[BUF_SIZE];
    show_err(sdb_init(&sdb, buf, BUF_SIZE, true),"init failure");

    for (uint8_t i=0;i<(sizeof(test_values) / sizeof(test_values[0]));i++) {
        show_err(sdb_add_int(&sdb,test_values[i].name,test_values[i].type,&test_values[i].value),"add failed");
    }

    const char *msg0 = "This is going to be a named blob.";
    show_err(
        sdb_add_blob(&sdb, "myblob", msg0, strlen(msg0)),
        "add blob failure"
    );

    const char *msg1 = "This is a different named blob.";
    show_err(
        sdb_add_blob(&sdb, "otherblob", msg1, strlen(msg1)),
        "add blob failure"
    );


    const char *msg2 = "This is some extra stuff";
    show_err(
        sdb_add_blob(&sdb, "blob3", msg2, strlen(msg2)),
        "add extra failure"
    );
  
    sdb_val_t fdata;
    sdbtypes_t ftype;
    const char *search_keys[] = {
        "andrew","bud","lewis","jeremy", 
    };
    for (uint8_t i=0;i<(sizeof(search_keys)/sizeof(search_keys[0]));i++) {
        const char *search_key = search_keys[i];
        sdb_val_t correct_answer = {0};
        for (uint8_t j=0;j<(sizeof(test_values)/sizeof(test_values[0]));j++) {
            if (!strcmp(search_key,test_values[j].name)) {
                correct_answer = test_values[j].value;
                break;
            }
        }
        fdata.u64 = 0;
        int8_t fail = show_err(sdb_get_int(&sdb, search_key, &ftype, &fdata),"did not find key");
        if (!fail) {
            show_err(check_ok(fdata,correct_answer,ftype),"wrong answer");
        }
    }

    uint8_t target[500];
    uint16_t act_len = 0; 
    
    show_err(sdb_get_blob(&sdb, "myblob", 500, target, &act_len),"blob not found");
    show_err(act_len != strlen(msg0),"blob length is incorrect");
    show_err(memcmp(target, msg0, act_len),"blob does not match");

    show_err(sdb_get_blob(&sdb, "blob3", 500, target, &act_len),"no extra data");
    show_err(act_len != strlen(msg2),"extra length is incorrect");
    show_err(memcmp(target, msg2, act_len),"extra bytes do not match");

    // update bud, it should work
    int16_t s16 = 1001;
    show_err(sdb_add_int(&sdb,"bud",SDB_S16, &s16),"change bud failed");
    fdata.u64 = 0;
    show_err(sdb_get_int(&sdb, "bud", &ftype, &fdata),"could not update bud");
    show_err(fdata.s16 != 1001,"bud not correct");

    // update david, but different type. should fail
    uint32_t u32 = 0x1a2b3c4d;    
    show_err(!sdb_add_int(&sdb,"david",SDB_U32, &u32),"change david should fail");

    // get seymour 
    show_err(!sdb_get_int(&sdb, "seymour", &ftype, &fdata),"could not find seymour");

    sdb_debug(&sdb);
    dumpBuffer(sdb.buf,sdb_size(&sdb));

    if (getErrors()) {
        printf("ERROR -- There were %d errors\n",getErrors());
    }

    FILE *fp = fopen("t0.dat","wb");
    fwrite(sdb.buf,1,sdb_size(&sdb),fp);
    fclose(fp);

    return getErrors();
};


int test_two() {

    sdb_t sdb;
    uint8_t buf[BUF_SIZE];
    show_err(sdb_init(&sdb, buf, BUF_SIZE, true),"init failure");

    const uint32_t test_elems = 30;    
    for (uint32_t i=0;i<test_elems;i++) {
        sdbtypes_t stype = (sdbtypes_t)(rand() % (uint16_t)_SDB_INVALID_TYPE);
        bool is_signed = false;
        if (stype <= SDB_S64) is_signed = true;

        char prename_buf[10];
        for (uint8_t i=0;i<10;i++) {
            prename_buf[i] = 'a' + rand() % ('z' - 'a');
        }
        prename_buf[5] = 0;

        char name_buf[80];
        snprintf(name_buf, 80, "%s_%03d_%d.%c", prename_buf, i, stype, is_signed ? 'i' : 'u');
        sdb_val_t rint;
        if (stype < SDB_FLOAT) {
            rint.u64 = 0;
            rint.u64 <<= 16; rint.u64 |= rand() & 0xffff;
            rint.u64 <<= 16; rint.u64 |= rand() & 0xffff;
            rint.u64 <<= 16; rint.u64 |= rand() & 0xffff;
            rint.u64 <<= 16; rint.u64 |= rand() & 0xffff;
        } else if (stype == SDB_FLOAT) {
            rint.f = ((float)rand() * (float)rand()) / (float)RAND_MAX;
        } else if (stype == SDB_DOUBLE) {
            rint.d = ((double)rand() * (double)rand()) / (double)RAND_MAX;
        }

        if (stype == SDB_BLOB) {
            uint8_t tbuf[256];
            uint32_t dcount = rand() & 0xff;
            for (uint32_t i=0;i<dcount;i++) {
                tbuf[i] = rand() & 0xff;
            }
            show_err(sdb_add_blob(&sdb,name_buf, &tbuf, dcount),"add blob failed");
        } else {
            show_err(sdb_add_int(&sdb,name_buf,stype,&rint),"add failed");
        }
    }

    sdb_debug(&sdb);
    dumpBuffer(sdb.buf,sdb_size(&sdb));

    FILE *fp = fopen("t1.dat","wb");
    fwrite(sdb.buf,1,sdb_size(&sdb),fp);
    fclose(fp);

    return getErrors();
};



int test_three() {
    // sdbufs inside other sdbufs!

    sdb_t outer;
    uint8_t outbuf[BUF_SIZE];
    show_err(sdb_init(&outer, outbuf, BUF_SIZE, true),"could not init top buf");

    for (uint8_t i=0;i<3;i++) {
        uint8_t inbuf[512];
        sdb_t inner;
        show_err(sdb_init(&inner, inbuf, 512, true),"could not init temp inner buf");
        int8_t i8 = 0 - 1 - i;
        uint16_t u16 = 1000 + i;
        int32_t i32 = -1000000 - i;
        char nbuf[10];
        snprintf(nbuf,10,"v%d.%d",i,0);
        sdb_add_int(&inner,nbuf,SDB_S8,&i8);
        snprintf(nbuf,10,"v%d.%d",i,1);
        sdb_add_int(&inner,nbuf,SDB_U16,&u16);
        snprintf(nbuf,10,"v%d.%d",i,2);
        sdb_add_int(&inner,nbuf,SDB_S32,&i32);
        snprintf(nbuf,10,"outer%d",i);
        show_err(sdb_add_blob(&outer,nbuf,inner.buf,sdb_size(&inner)),"could not add inner buf as blob");
    }

    sdb_debug(&outer);
    dumpBuffer(outer.buf,sdb_size(&outer));

    for (uint8_t i=2;i<3;i--) {
        char nbuf[10];
        snprintf(nbuf,10,"outer%d",i);
        uint8_t ibuf[512];
        sdb_len_t act_len = 0;
        show_err(sdb_get_blob(&outer,nbuf,512,ibuf,&act_len),"sub blob not found");
        sdb_t inner;
        show_err(sdb_init(&inner,ibuf,act_len,false),"init err");
        sdb_debug(&inner);
        dumpBuffer(inner.buf,sdb_size(&inner));
        sdb_val_t val;
        sdbtypes_t rtype;

        snprintf(nbuf,10,"v%d.%d",i,0);
        show_err(sdb_get_int(&inner,nbuf,&rtype,&val),"not found");
        show_err(val.s8 != 0 -1 -i, "wrong value");
        snprintf(nbuf,10,"v%d.%d",i,1);
        show_err(sdb_get_int(&inner,nbuf,&rtype,&val),"not found");
        show_err(val.u16 != 1000 + i, "wrong value");
        snprintf(nbuf,10,"v%d.%d",i,2);
        show_err(sdb_get_int(&inner,nbuf,&rtype,&val), "not_found");
        show_err(val.s32 != -1e6 - i, "wrong value");
    }
    FILE *fp = fopen("t2.dat","wb");
    fwrite(outer.buf,1,sdb_size(&outer),fp);
    fclose(fp);

    return getErrors();
};



int main(int argc, char *argv[]) {
    int32_t rv0 = test_one();
    int32_t rv1 = test_two();
    int32_t rv2 = test_three();
    return rv0 + rv1 + rv2;
}
