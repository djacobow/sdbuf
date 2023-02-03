#include <stdint.h>
#include <random>
#include <map>
#include <vector>
#include <stdio.h>

#include "sdbuf.h"
#include "debug.h"


uint32_t test_one() {

    uint32_t errors = 0;
    const uint32_t buf_size = 1024;
    uint8_t buf[buf_size] = {};

    for (uint32_t iters=0; iters < 100; iters++) {

        sdb_t c = {};
        sdb_init(&c, buf, buf_size, true);

        std::map<uint16_t, uint64_t> ref = {};

        for (uint32_t i=0; i< 90; i++) {
            uint16_t k = rand() & 0xffff;
            uint64_t v = 0;
            uint32_t len = rand() & 0x7;
            for (uint32_t j=0; j<=len; j++) {
                v <<= 8;
                v |= rand() & 0xff;
            }
            ref[k] = v;
            if (int8_t err = sdb_set_unsigned(&c, k, v) != SDB_OK) {
                printf("Could not add, err: %d\n", err);
                errors++;
            }
        }

        // select a few to delete
        std::vector<uint16_t> ref_keys;

        for (auto const& e: ref) {
            auto r = rand() && rand() & 0x7;
            if (!r) {
                ref_keys.push_back(e.first);
            }
        }
        
        for (const auto &f: ref_keys) {
            ref.erase(f);
            if (int8_t err = sdb_remove(&c, f) != SDB_OK) {
                printf("Could not remove, err: %d\n", err); fflush(stdout);
                errors++;
            }
        }

        for (const auto &e : ref) {
            int8_t error = 0;
            auto t_val = sdb_get_unsigned(&c, e.first, &error);
            auto r_val = e.second;
            if (t_val != r_val) {
                printf("t %lx r %lx\n", t_val, r_val);
                errors++;
            }
            if (error) {
                errors++;
            }
        }

//        sdb_debug(&c);
    }
         
    printf("ERR COUNT %u\n", errors);
    return errors;
}



int main(int argc, const char *argv[]) {
    uint32_t errors = 0;
    errors += test_one();
    if (errors) {
        printf("FAIL. There were %u errors\n", errors);
        return errors;
    }
    printf("PASS!\n");
    return 0;
}
