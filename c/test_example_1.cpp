
#include <ctype.h>
#include <map>
#include <random>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "sdbuf.h"

#define BUF_SIZE (2048)

class ErrorCounter_c {
    private:
        uint32_t errors;
    public:
        ErrorCounter_c() : errors(0) {};
        uint32_t get() { return errors; }
        int8_t check(int8_t rv, const char *msg) {
            if (rv) {
                errors++;
                printf("err: %s, rv: %d\n",msg,rv);
            } 
            return rv;
        }
};

static ErrorCounter_c ec;

void dump(void *p, uint32_t l) {
    auto p8 = static_cast<uint8_t *>(p);
    for (uint32_t i=0;i<l;i++) {
        printf("%02x", *(p8 + i));
        if ((i & 0x1f) == 0x1f) {
            printf("\n");
        }
    }
    printf("\n");
}
typedef struct test_value_t {
    public:
        const uint16_t id;
        sdbtypes_t type;
        sdb_val_t  value;

        test_value_t(uint16_t nid, sdbtypes_t ntype, sdb_val_t nval) :
            id(nid), type(ntype), value(nval) {};
        bool compare(const test_value_t &ref) {
            if (type != ref.type) return false;
            switch (type) {
                case SDB_S8:  if (value.s8  != ref.value.s8)  return false; break;
                case SDB_U8:  if (value.u8  != ref.value.u8)  return false; break;
                case SDB_S16: if (value.s16 != ref.value.s16) return false; break;
                case SDB_U16: if (value.u16 != ref.value.u16) return false; break;
                case SDB_S32: if (value.s32 != ref.value.s32) return false; break;
                case SDB_U32: if (value.u32 != ref.value.u32) return false; break;
                case SDB_S64: if (value.s64 != ref.value.s64) return false; break;
                case SDB_U64: if (value.u64 != ref.value.u64) return false; break;
                default: break;
            }
            return true;
        }
} test_value_t;


int test_one() {

    std::vector<test_value_t> test_values = {
        test_value_t(0x0100, SDB_S16, sdb_val_t{.s16 = 0x55aa}),
        test_value_t(0x0101, SDB_U32, sdb_val_t{.u32 = 0x12345678}),
        test_value_t(0x0102, SDB_S16, sdb_val_t{.s16 = 0x7006}),
        test_value_t(0x0103, SDB_U32, sdb_val_t{.u32 = 0xdeadbeef}),
        test_value_t(0x0104, SDB_U64, sdb_val_t{.u64 = 0xaabbccdd99887766ULL}),
        test_value_t(0x0105, SDB_S16, sdb_val_t{.s16 = 0x6007}),
        test_value_t(0x0106, SDB_S8,  sdb_val_t{.s8 = -12})
    };

    sdb_t sdb;
    uint8_t buf[BUF_SIZE];
    ec.check(sdb_init(&sdb, buf, BUF_SIZE, true),"init failure");

    for (const auto &e: test_values) {
        ec.check(sdb_set_val(&sdb, e.id, e.type, &e.value),"add failed");
    }

    const char *msg0 = "This is going to be a named blob.";
    ec.check(
        sdb_add_blob(&sdb, 0x0201, msg0, strlen(msg0)),
        "add blob failure"
    );

    const char *msg1 = "This is a different named blob.";
    ec.check(
        sdb_add_blob(&sdb, 0x0202, msg1, strlen(msg1)),
        "add blob failure"
    );


    const char *msg2 = "This is some extra stuff";
    ec.check(
        sdb_add_blob(&sdb, 0x0203, msg2, strlen(msg2)),
        "add extra failure"
    );
  
    sdb_val_t fdata;
    const std::vector<uint16_t> search_keys = {
        0x105, 0x102, 0x104, 0x0103,
    };

    for (const auto &sk: search_keys) {
        auto correct_answer = test_values[sk];
        fdata.u64 = 0;
        auto mi = sdb_find(&sdb, sk);
        if (!mi.valid) {
            int8_t fail = ec.check(sdb_get(&mi,&fdata),"problem fetching data");
            if (fail != SDB_OK) {
                ec.check(correct_answer.compare(test_value_t(mi.id, mi.type, fdata)), "value mismatch");
            }
        }
    }

    uint8_t target[500];

    auto mi = sdb_find(&sdb, 0x201);
    ec.check(!mi.valid, "blob not found");
    ec.check(mi.elemsize != strlen(msg0),"blob length is not correct");
    ec.check(sdb_get(&mi,target),"problem fetching");
    ec.check(memcmp(target, msg0, mi.elemsize),"blob 0x201 does not match");

    mi = sdb_find(&sdb, 0x203);
    ec.check(!mi.valid, "blob3 0x203 not found");
    ec.check(mi.elemsize != strlen(msg2),"blob 0x203 length is not correct");
    ec.check(sdb_get(&mi,target),"problem fetching");
    ec.check(memcmp(target, msg2, mi.elemsize),"blob 0x203 does not match");

    int16_t s16 = 1001;
    ec.check(sdb_set_val(&sdb, 0x102, SDB_S16, &s16),"change 0x102 failed");
    fdata.u64 = 0;
    mi = sdb_find(&sdb, 0x102);
    ec.check(!mi.valid, "could not find 0x102");
    ec.check(sdb_get(&mi,&fdata),"could not get 0x102");
    ec.check(fdata.s16 != 1001,"0x102 not correct");

    // update but different type
    uint32_t u32 = 0x1a2b3c4d;    
    ec.check(sdb_set_val(&sdb, 0x106,SDB_U32, &u32),"failed to update 0x106");
    mi = sdb_find(&sdb, 0x106);
    ec.check(!mi.valid, "0x106 not found");
    ec.check(sdb_get(&mi,&fdata),"could read 0x106");
    ec.check(fdata.u32 != u32, "0x106 not correct");

    // get 0x1000
    mi = sdb_find(&sdb, 0x1000);
    ec.check(mi.valid, "should not find 0x1000");

    sdb_debug(&sdb);
    dump(sdb.buf,sdb_size(&sdb));

    if (ec.get()) {
        printf("ERROR -- There were %d errors\n",ec.get());
    }

    FILE *fp = fopen("t0.dat","wb");
    fwrite(sdb.buf,1,sdb_size(&sdb),fp);
    fclose(fp);

    return ec.get();
};


int test_two() {

    sdb_t sdb;
    uint8_t buf[BUF_SIZE];
    ec.check(sdb_init(&sdb, buf, BUF_SIZE, true),"init failure");

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
            ec.check(sdb_add_blob(&sdb, id, &tbuf, dcount),"add blob failed");
        } else {
#if (!SDB_INCL_FLOAT)
            if ((stype != SDB_FLOAT) && (stype != SDB_DOUBLE)) {
#endif
            ec.check(sdb_set_val(&sdb, id, stype,&rint),"add failed");
#if (!SDB_INCL_FLOAT)
            }
#endif
        }
    }

    sdb_debug(&sdb);
    dump(sdb.buf,sdb_size(&sdb));

    FILE *fp = fopen("t1.dat","wb");
    fwrite(sdb.buf,1,sdb_size(&sdb),fp);
    fclose(fp);

    return ec.get();
};



int test_three() {
    // sdbufs inside other sdbufs!

    sdb_t outer;
    uint8_t outbuf[BUF_SIZE];
    ec.check(sdb_init(&outer, outbuf, BUF_SIZE, true),"could not init top buf");

    for (uint8_t i=0;i<3;i++) {
        uint8_t inbuf[512];
        sdb_t inner;
        ec.check(sdb_init(&inner, inbuf, 512, true),"could not init temp inner buf");
        int8_t i8 = 0 - 1 - i;
        uint16_t u16 = 1000 + i;
        int32_t i32 = -1000000 - i;
        sdb_set_val(&inner, i*16 + 0, SDB_S8,&i8);
        sdb_set_val(&inner, i*16 + 1, SDB_U16,&u16);
        sdb_set_val(&inner, i*16 + 2, SDB_S32,&i32);
        ec.check(sdb_add_blob(&outer,i*256,inner.buf,sdb_size(&inner)),"could not add inner buf as blob");
    }

    sdb_debug(&outer);
    dump(outer.buf,sdb_size(&outer));

    for (uint8_t i=2;i<3;i--) {
        auto mi = sdb_find(&outer, i*256);
        ec.check(!mi.valid, "sub blob not found");
        uint8_t ibuf[512] = {};
        ec.check(sdb_get(&mi,ibuf),"sub blob not found");

        sdb_t inner = {};
        ec.check(sdb_init(&inner, ibuf, mi.elemsize, false),"init err");
        sdb_debug(&inner);
        dump(inner.buf,sdb_size(&inner));
        sdb_val_t val;

        mi = sdb_find(&inner, i*16 + 0);
        sdb_show_mi(&mi);
        ec.check(!mi.valid, "not found A");
        ec.check(sdb_get(&mi,&val),"get fail A");
        ec.check(val.s8 != 0 -1 -i, "wrong value A");

        mi = sdb_find(&inner, i*16 + 1);
        ec.check(!mi.valid, "not found B");
        ec.check(sdb_get(&mi,&val),"get fail B");
        ec.check(val.u16 != 1000 + i, "wrong value B");

        mi = sdb_find(&inner, i*16 + 2);
        ec.check(!mi.valid, "not found C");
        ec.check(sdb_get(&mi,&val),"get fail  C");
        ec.check(val.s32 != -1e6 - i, "wrong value C");
    }
    FILE *fp = fopen("t2.dat","wb");
    fwrite(outer.buf,1,sdb_size(&outer),fp);
    fclose(fp);

    return ec.get();
};

int test_four() {

    sdb_t s;
    uint8_t obuf[BUF_SIZE];
    ec.check(sdb_init(&s, obuf, BUF_SIZE, true),"could not init top buf");
    const uint8_t alen = 12;
    uint8_t u8[alen];
    int16_t i16[alen];
    uint32_t u32[alen];
    for (uint8_t i=0;i<alen;i++) {
        u8[i] = i;
        i16[i] = -i*1000;
        u32[i] = i*1000000;
    }
    ec.check(sdb_set_vala(&s, 0x1000, SDB_U8,  alen, u8), "could not add u8 arry");
    ec.check(sdb_set_vala(&s, 0x1001, SDB_S16, alen, i16),"could not add i16 arry");
    ec.check(sdb_set_vala(&s, 0x1002, SDB_U32, alen, u32),"could not add u32 arry");

    sdb_debug(&s);
    dump(s.buf,sdb_size(&s));

    FILE *fp = fopen("t3.dat","wb");
    fwrite(s.buf,1,sdb_size(&s),fp);
    fclose(fp);

    return ec.get();
};

int test_five() {
    sdb_t s;
    uint8_t obuf[BUF_SIZE];
    ec.check(sdb_init(&s, obuf, BUF_SIZE, true),"could not init top buf");

   
    sdb_val_t v;
    v.u8 = 0x11;
    ec.check(sdb_set_val(&s, 0x1000, SDB_U8,  &v), "could not add u8");
    v.s16 = 0x2222;
    ec.check(sdb_set_val(&s, 0x1001, SDB_S16, &v),"could not add i16");
    v.u32 = 0x33333333;
    ec.check(sdb_set_val(&s, 0x1002, SDB_U32, &v),"could not add u32");
    v.u8 = 0x44;
    ec.check(sdb_set_val(&s, 0x2000, SDB_U8,  &v), "could not add u8");
    v.s16 = 0x5555;
    ec.check(sdb_set_val(&s, 0x2001, SDB_S16, &v),"could not add i16");
    v.u32 = 0x66666666;
    ec.check(sdb_set_val(&s, 0x2002, SDB_U32, &v),"could not add u32");

    sdb_debug(&s);

    ec.check(sdb_remove(&s, 0x1001), "could not remove");;
    ec.check(sdb_remove(&s, 0x2000), "could not remove");;
    ec.check(sdb_remove(&s, 0x2002), "could not remove");;

    sdb_debug(&s);

    dump(s.buf,sdb_size(&s));

    FILE *fp = fopen("t5.dat","wb");
    fwrite(s.buf,1,sdb_size(&s),fp);
    fclose(fp);

    return ec.get();
};


int test_six() {
    const uint32_t obsize = 100 * 1024;
    sdb_t s;
    uint8_t obuf[obsize];
    memset(obuf, 0xee, obsize);
    ec.check(sdb_init(&s, obuf, obsize, true),"could not init top buf");

    typedef std::map<uint16_t, std::string> blobmap_t;
    blobmap_t bm;

    uint32_t blob_count = 100;
    for (uint32_t i=0; i<blob_count; i++) {
        uint16_t id = i << 8;
        uint32_t blob_len = rand() & 0x3ff;
        std::string r = {};
        r.resize(blob_len);
        for (uint32_t k=0;k<blob_len;k++) { r[k] = rand() & 0xff; };
        bm[id] = r;
        ec.check(sdb_add_blob(&s, id, r.data(), r.size()), "could not add blob");
    }

    sdb_debug(&s);

    dump(s.buf,sdb_size(&s));

    FILE *fp = fopen("t6.dat","wb");
    fwrite(s.buf,1,sdb_size(&s),fp);
    fclose(fp);

    for (const auto &e: bm) {
        auto mi = sdb_find(&s, e.first);
        if (ec.check(!mi.valid, "could not find item")) {
            auto tbuf = alloca(mi.minsize);
            ec.check(sdb_get(&mi, tbuf), "could not copy data");
            ec.check(memcmp(e.second.data(), tbuf, mi.minsize), "data do not match");
        }
    }
   
    fp = fopen("t6.dat", "rb");
    fseek(fp, 0L, SEEK_END); 
    size_t nbsize = ftell(fp);
    void *nbuf = alloca(nbsize);
    fseek(fp, 0, SEEK_SET);
    fread(nbuf, 1, nbsize, fp);
    fclose(fp);

    sdb_t t;
    ec.check(sdb_init(&t, nbuf, nbsize, false),"could not init second buf");

    for (const auto &e: bm) {
        auto mi = sdb_find(&s, e.first);
        if (ec.check(!mi.valid, "could not find item (clone)")) {
            auto tbuf = alloca(mi.minsize);
            ec.check(sdb_get(&mi, tbuf), "could not copy data (clone)");
            ec.check(memcmp(e.second.data(), tbuf, mi.minsize), "data do not match (clone)");
        }
    }
   
    return ec.get();
}


int main(int argc, char *argv[]) {
    test_one();
    test_two();
    test_three();
    test_four();
    test_five();
    test_six();

    uint32_t e = ec.get();
    if (e) {
        printf("FAIL! (%s) There were %u errors\n", argv[0], e);
    } else {
        printf("PASS!  (%s) Yay!\n", argv[0]);
    }
    return e;
}

