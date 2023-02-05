#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <random>
#include <map>
#include <vector>
#include <utility>

#include "sdbuf.h"

class t1 {

    private:

        std::map<uint16_t, std::pair<sdb_val_t, sdbtypes_t>> iref;
        std::map<uint16_t, std::vector<uint16_t>> aref;
        static const uint32_t buf_size = 1536;
        uint8_t buf[buf_size] = {};
        sdb_t c;

        uint32_t errors = 0;
        uint32_t iters;

    public:

    t1(uint32_t iters) : iters(iters) {};

    void addSomeInts(uint32_t count, bool make_signed) {
        for (uint32_t i=0; i< count; i++) {
            uint16_t k = 0;
            do {
                k = rand() & 0xffff;
            } while (aref.count(k) != 0);
            sdb_val_t v = {};
            sdbtypes_t t = static_cast<sdbtypes_t>(rand() & 0x7);

            uint32_t len = 1 << (t & 0x3);
            for (uint32_t j=0; j<len; j++) {
                v.u64  <<= 8;
                v.u64 |= rand() & 0xff;
            }
            bool do_unsigned = t & 0x4;

            iref[k] = {v, t};

            if (do_unsigned) {
                if (int8_t err = sdb_set_unsigned(&c, k, v.u64) != SDB_OK) {
                    printf("Could not add, err: %d\n", err);
                    errors++;
                }
            } else {
                if (int8_t err = sdb_set_signed(&c, k, v.s64) != SDB_OK) {
                    printf("Could not add, err: %d\n", err);
                    errors++;
                }
            }
        }
    }

    void addSomeArrays(uint32_t count, uint32_t len_mask) {
        for (uint32_t i=0; i<count; i++) {
            uint16_t k = 0;
            do {
                k = rand() & 0xffff;
            } while (iref.count(k) != 0);
            uint16_t len = rand() & len_mask;
            std::vector<uint16_t> v;
            for (uint32_t j=0;j<len;j++) {
                v.push_back(rand() & 0xffff);
            }
            aref[k] = v;
            if (SDB_OK != sdb_set_vala(&c, k, SDB_U16, len, v.data())) {
                printf("failed to add blob at id %u len %u\n", k, len);
                errors++;
            }
        }
    }
    void deleteSomeKeys(uint16_t mask) {
        // select a few to delete
        std::vector<uint16_t> ref_keys;
    
        for (auto const& e: iref) {
            auto r = rand() && rand() & 0x7;
            if (!r) {
                ref_keys.push_back(e.first);
            }
        }
        
        for (const auto &f: ref_keys) {
            iref.erase(f);
            if (int8_t err = sdb_remove(&c, f) != SDB_OK) {
                printf("Could not remove %x, err: %d\n", f, err); fflush(stdout);
                errors++;
            }
        }
    }

    void compareArraysToRef() {
        for (const auto &e : aref) {
            auto mi = sdb_find(&c, e.first);
            if (!mi.valid) {
                printf("blob key %u not found\n", e.first);
                errors++;
            }
            uint16_t t_val[0xf] = {};
            if (SDB_OK != sdb_get(&mi, &t_val)) {
                printf("blob get failed\n");
                errors++;
            }
            if (memcmp(t_val, aref[e.first].data(), mi.minsize)) {
                printf("blob does not match id %x\n", e.first);
                errors++;
            }
        }
    }

    void compareIntsToRef() {
        for (const auto &e : iref) {
            int8_t error = 0;
            if ((e.second.second >= SDB_U8) && (e.second.second <= SDB_U64)) {
                auto t_val = sdb_get_unsigned(&c, e.first, &error);
                auto r_val = e.second.first;
                if (t_val != r_val.u64) {
                    printf("U id %x t %lx r %lx\n", e.first, t_val, r_val.u64);
                    errors++;
                }
            } else if ((e.second.second >= SDB_S8) && (e.second.second <= SDB_S64)) {
                auto t_val = sdb_get_signed(&c, e.first, &error);
                auto r_val = e.second.first;
                if (t_val != r_val.s64) {
                    printf("S id %x t %lx r %lx err %d\n", e.first, t_val, r_val.s64, error);
                    errors++;
                }
            }

            if (error) {
                errors++;
            }
        }
    }

    uint32_t go() {
        for (uint32_t iter=0; iter < iters; iter++) {
            sdb_init(&c, buf, buf_size, true);
            addSomeInts(90, true);
            deleteSomeKeys(0x7);
            addSomeArrays(10, 0xf);
            addSomeInts(10, true);
            deleteSomeKeys(0x7);
            addSomeInts(20, true);
            deleteSomeKeys(0x7);
            addSomeArrays(10, 0xf);
            compareIntsToRef(); 
            compareArraysToRef();
            // sdb_debug(&c);
            iref.clear();
            aref.clear();
        }
         
        printf("ERR COUNT %u\n", errors);
        return errors;
    }
};



int main(int argc, const char *argv[]) {
    auto errors = t1(100).go();
    if (errors) {
        printf("FAIL.  (%s) There were %u errors\n", argv[0], errors);
        return errors;
    }
    printf("PASS!  (%s) Yay!\n", argv[0]);
    return 0;
}
