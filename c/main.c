
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
    uint64_t   value;
} test_value_t;

test_value_t test_values[] = {
    {
        .name = "james",
        .type = SDB_S16,
        .value = 0x55aa,
    },
    {
        .name = "ido",
        .type = SDB_U32,
        .value = 0x12345678,
    },
    {
        .name = "bud",
        .type = SDB_S16,
        .value = 0xf00d,
    },
    {
        .name = "jeremy",
        .type = SDB_U32,
        .value = 0xdeadbeef,
    },
    {
        .name = "lewis",
        .type = SDB_U64,
        .value = 0xaabbccdd99887766ULL,
    },
    {
        .name = "andrew",
        .type = SDB_S16,
        .value = 0xd00f,
    },
    {
        .name = "david",
        .type = SDB_S8,
        .value = (uint64_t)(-12LL),
    },
};


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
  
    uint64_t fdata;
    sdbtypes_t ftype;
    const char *search_keys[] = {
        "andrew","bud","lewis","jeremy", 
    };
    for (uint8_t i=0;i<(sizeof(search_keys)/sizeof(search_keys[0]));i++) {
        const char *search_key = search_keys[i];
        uint64_t correct_answer = 0;
        for (uint8_t j=0;j<(sizeof(test_values)/sizeof(test_values[0]));j++) {
            if (!strcmp(search_key,test_values[j].name)) {
                correct_answer = test_values[j].value;
                break;
            }
        }
        fdata = 0;
        int8_t fail = show_err(sdb_get_int(&sdb, search_key, &ftype, &fdata),"did not find key");
        if (!fail) {
            show_err(fdata != correct_answer, "wrong answer");
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
    int16_t s16 = 0xaa55;
    show_err(sdb_add_int(&sdb,"bud",SDB_S16, &s16),"change bud failed");
    fdata = 0;
    show_err(sdb_get_int(&sdb, "bud", &ftype, &fdata),"could not update bud");
    show_err(fdata != 0xaa55,"bud not correct");

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
        uint64_t rint = 0;
        rint <<= 16; rint |= rand() & 0xffff;
        rint <<= 16; rint |= rand() & 0xffff;
        rint <<= 16; rint |= rand() & 0xffff;
        rint <<= 16; rint |= rand() & 0xffff;

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



int main(int argc, char *argv[]) {
    int32_t rv0 = test_one();
    int32_t rv1 = test_two();
    return rv0 + rv1;
}
