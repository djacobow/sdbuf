
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "debug.h"
#include "sdbuf.h"

#define BUF_SIZE (2048)

typedef struct test_value_t {
    const uint16_t id;
    sdbtypes_t type;
    sdb_val_t  value;
} test_value_t;

test_value_t test_values[] = {
    { .id = 0x0100, .type = SDB_S16, .value.s16 = 0x55aa, },
    { .id = 0x0101, .type = SDB_U32, .value.u32 = 0x12345678, },
    { .id = 0x0102, .type = SDB_S16, .value.s16 = 0xf00d, },
    { .id = 0x0103, .type = SDB_U32, .value.u32 = 0xdeadbeef, },
    { .id = 0x0104, .type = SDB_U64, .value.u64 = 0xaabbccdd99887766ULL, },
    { .id = 0x0105, .type = SDB_S16, .value.s16 = 0xd00f, },
    { .id = 0x0106, .type = SDB_S8,  .value.s8 = -12, },
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
        show_err(sdb_set_val(&sdb,test_values[i].id,test_values[i].type,&test_values[i].value),"add failed");
    }

    const char *msg0 = "This is going to be a named blob.";
    show_err(
        sdb_add_blob(&sdb, 0x0201, msg0, strlen(msg0)),
        "add blob failure"
    );

    const char *msg1 = "This is a different named blob.";
    show_err(
        sdb_add_blob(&sdb, 0x0202, msg1, strlen(msg1)),
        "add blob failure"
    );


    const char *msg2 = "This is some extra stuff";
    show_err(
        sdb_add_blob(&sdb, 0x0203, msg2, strlen(msg2)),
        "add extra failure"
    );
  
    sdb_val_t fdata;
    const uint16_t search_keys[] = {
        0x105, 0x102, 0x104, 0x0103,
    };
    for (uint8_t i=0;i<(sizeof(search_keys)/sizeof(search_keys[0]));i++) {
        const uint16_t search_key = search_keys[i];
        sdb_val_t correct_answer = {0};
        for (uint8_t j=0;j<(sizeof(test_values)/sizeof(test_values[0]));j++) {
            if (search_key != test_values[j].id) {
                correct_answer = test_values[j].value;
                break;
            }
        }
        fdata.u64 = 0;
        sdb_member_info_t mi;
        int8_t fail = show_err(sdb_find(&sdb, search_key, &mi),"did not find key");
        if (!fail) {
            fail = show_err(sdb_get(&mi,&fdata),"problem fetching data");
            if (fail != SDB_OK) {
                show_err(check_ok(fdata,correct_answer,mi.type),"wrong answer");
            }
        }
    }

    uint8_t target[500];
    sdb_member_info_t mi; 

    show_err(sdb_find(&sdb, 0x201, &mi),"blob 0x201 not found");
    show_err(mi.elemsize != strlen(msg0),"blob length is not correct");
    show_err(sdb_get(&mi,target),"problem fetching");
    show_err(memcmp(target, msg0, mi.elemsize),"blob 0x201 does not match");

    show_err(sdb_find(&sdb, 0x203, &mi),"blob3 0x203 not found");
    show_err(mi.elemsize != strlen(msg2),"blob 0x203 length is not correct");
    show_err(sdb_get(&mi,target),"problem fetching");
    show_err(memcmp(target, msg2, mi.elemsize),"blob 0x203 does not match");

    int16_t s16 = 1001;
    show_err(sdb_set_val(&sdb, 0x102, SDB_S16, &s16),"change 0x102 failed");
    fdata.u64 = 0;
    show_err(sdb_find(&sdb, 0x102, &mi),"could not find 0x102");
    show_err(sdb_get(&mi,&fdata),"could not get 0x102");
    show_err(fdata.s16 != 1001,"0x102 not correct");

    // update but different type
    uint32_t u32 = 0x1a2b3c4d;    
    show_err(sdb_set_val(&sdb, 0x106,SDB_U32, &u32),"failed to update 0x106");
    show_err(sdb_find(&sdb, 0x106, &mi),"0x106 not found");
    show_err(sdb_get(&mi,&fdata),"could read 0x106");
    show_err(fdata.u32 != u32, "0x106 not correct");

    // get 0x1000
    show_err(!sdb_find(&sdb, 0x1000, &mi),"should not find 0x1000");

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

        uint16_t id = rand() & 0xffff;
        
        sdb_val_t rint;
        if (stype < SDB_FLOAT) {
            rint.u64 = 0;
            rint.u64 <<= 16; rint.u64 |= rand() & 0xffff;
            rint.u64 <<= 16; rint.u64 |= rand() & 0xffff;
            rint.u64 <<= 16; rint.u64 |= rand() & 0xffff;
            rint.u64 <<= 16; rint.u64 |= rand() & 0xffff;
#if SDB_INCL_FLOAT
        } else if (stype == SDB_FLOAT) {
            rint.f = ((float)rand() * (float)rand()) / (float)RAND_MAX;
        } else if (stype == SDB_DOUBLE) {
            rint.d = ((double)rand() * (double)rand()) / (double)RAND_MAX;
#endif
        }

        if (stype == SDB_BLOB) {
            uint8_t tbuf[256];
            uint32_t dcount = rand() & 0xff;
            for (uint32_t i=0;i<dcount;i++) {
                tbuf[i] = rand() & 0xff;
            }
            show_err(sdb_add_blob(&sdb, id, &tbuf, dcount),"add blob failed");
        } else {
#if (!SDB_INCL_FLOAT)
            if ((stype != SDB_FLOAT) && (stype != SDB_DOUBLE)) {
#endif
            show_err(sdb_set_val(&sdb, id, stype,&rint),"add failed");
#if (!SDB_INCL_FLOAT)
            }
#endif
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
        sdb_set_val(&inner, i*16 + 0, SDB_S8,&i8);
        sdb_set_val(&inner, i*16 + 1, SDB_U16,&u16);
        sdb_set_val(&inner, i*16 + 2, SDB_S32,&i32);
        show_err(sdb_add_blob(&outer,i*256,inner.buf,sdb_size(&inner)),"could not add inner buf as blob");
    }

    sdb_debug(&outer);
    dumpBuffer(outer.buf,sdb_size(&outer));

    for (uint8_t i=2;i<3;i--) {
        sdb_member_info_t mi;
        show_err(sdb_find(&outer, i*256, &mi),"sub blob not found");
        uint8_t ibuf[512] = {};
        show_err(sdb_get(&mi,ibuf),"sub blob not found");

        sdb_t inner = {};
        show_err(sdb_init(&inner, ibuf, mi.elemsize, false),"init err");
        sdb_debug(&inner);
        dumpBuffer(inner.buf,sdb_size(&inner));
        sdb_val_t val;

        show_err(sdb_find(&inner, i*16 + 0, &mi),"not found");
        show_err(sdb_get(&mi,&val),"not found");
        show_err(val.s8 != 0 -1 -i, "wrong value");

        show_err(sdb_find(&inner, i*16 + 1, &mi),"not found");
        show_err(sdb_get(&mi,&val),"not found");
        show_err(val.u16 != 1000 + i, "wrong value");

        show_err(sdb_find(&inner, i*16 + 2, &mi),"not found");
        show_err(sdb_get(&mi,&val),"not found");
        show_err(val.s32 != -1e6 - i, "wrong value");
    }
    FILE *fp = fopen("t2.dat","wb");
    fwrite(outer.buf,1,sdb_size(&outer),fp);
    fclose(fp);

    return getErrors();
};

int test_four() {

    sdb_t s;
    uint8_t obuf[BUF_SIZE];
    show_err(sdb_init(&s, obuf, BUF_SIZE, true),"could not init top buf");
    const uint8_t alen = 12;
    uint8_t u8[alen];
    int16_t i16[alen];
    uint32_t u32[alen];
    for (uint8_t i=0;i<alen;i++) {
        u8[i] = i;
        i16[i] = -i*1000;
        u32[i] = i*1000000;
    }
    show_err(sdb_set_vala(&s, 0x1000, SDB_U8,  alen, u8), "could not add u8 arry");
    show_err(sdb_set_vala(&s, 0x1001, SDB_S16, alen, i16),"could not add i16 arry");
    show_err(sdb_set_vala(&s, 0x1002, SDB_U32, alen, u32),"could not add u32 arry");

    sdb_debug(&s);
    dumpBuffer(s.buf,sdb_size(&s));

    FILE *fp = fopen("t3.dat","wb");
    fwrite(s.buf,1,sdb_size(&s),fp);
    fclose(fp);

    return getErrors();
};

int test_five() {
    sdb_t s;
    uint8_t obuf[BUF_SIZE];
    show_err(sdb_init(&s, obuf, BUF_SIZE, true),"could not init top buf");

   
    sdb_val_t v;
    v.u8 = 0x11;
    show_err(sdb_set_val(&s, 0x1000, SDB_U8,  &v), "could not add u8");
    v.s16 = 0x2222;
    show_err(sdb_set_val(&s, 0x1001, SDB_S16, &v),"could not add i16");
    v.u32 = 0x33333333;
    show_err(sdb_set_val(&s, 0x1002, SDB_U32, &v),"could not add u32");
    v.u8 = 0x44;
    show_err(sdb_set_val(&s, 0x2000, SDB_U8,  &v), "could not add u8");
    v.s16 = 0x5555;
    show_err(sdb_set_val(&s, 0x2001, SDB_S16, &v),"could not add i16");
    v.u32 = 0x66666666;
    show_err(sdb_set_val(&s, 0x2002, SDB_U32, &v),"could not add u32");

    sdb_debug(&s);

    show_err(sdb_remove(&s, 0x1001), "could not remove");;
    show_err(sdb_remove(&s, 0x2000), "could not remove");;
    show_err(sdb_remove(&s, 0x2002), "could not remove");;

    sdb_debug(&s);

    dumpBuffer(s.buf,sdb_size(&s));

    FILE *fp = fopen("t5.dat","wb");
    fwrite(s.buf,1,sdb_size(&s),fp);
    fclose(fp);

    return getErrors();
};


int main(int argc, char *argv[]) {
    test_one();
    test_two();
    test_three();
    test_four();
    test_five();

    uint32_t e = getErrors();
    if (e) {
        printf("FAIL! There were %u errors\n", e);
    } else {
        printf("YAY! Pass!\n");
    }
    return e;
}

