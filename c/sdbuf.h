#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_NAME_LEN  (15)
#define SDB_VER_MAJOR (0x1)
#define SDB_VER_MINOR (0x1)

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t  sdb_hdr_t;
typedef uint16_t sdb_len_t;

// this type is not necessary, but useful as
// a shorthand convenience when "getting"
// integers
typedef union sdb_val_t {
    uint8_t  u8;
    int8_t   s8;
    uint16_t u16;
    int16_t  s16;
    uint32_t u32;
    int32_t  s32;
    uint64_t u64;
    int64_t  s64;
} sdb_val_t;

typedef enum __attribute__((__packed__)) sdbtypes_t {
    SDB_S8, SDB_S16, SDB_S32, SDB_S64,
    SDB_U8, SDB_U16, SDB_U32, SDB_U64,
    SDB_BLOB,
    _SDB_INVALID_TYPE,
} sdbtypes_t;

typedef enum sdb_errors_t {
    SDB_OK = 0,
    SDB_BUFFER_TOO_SMALL,
    SDB_DIFFERENT_TYPE,
    SDB_NOT_FOUND,
    SDB_DIFFERENT_SIZE,
    SDB_NAME_TOO_LONG,
    SDB_WRONG_VERSION,
} sdb_errors_t;

typedef struct sdb_t {
    void *buf;
    sdb_len_t len;
    sdb_hdr_t header;
    sdb_len_t vals_size;
} sdb_t;

// initialize the struct with the target buffer, optionally
// zero it out
int8_t   sdb_init     (sdb_t *sdb, void *b, const sdb_len_t l, bool clear);

// total size of blob
sdb_len_t sdb_size    (sdb_t *sdb);

// setter getter for ints
int8_t   sdb_add_int  (sdb_t *sdb, const char *name, const sdbtypes_t type, const void *data);
int8_t   sdb_get_int  (sdb_t *sdb, const char *name, sdbtypes_t *type, void *data);

// setter getter for blobs
int8_t   sdb_add_blob (sdb_t *sdb, const char *name, const void *ib, const sdb_len_t ilen);
int8_t   sdb_get_blob (sdb_t *sdb, const char *name, const sdb_len_t omax, void *ob, sdb_len_t *olen);

// dumper
void     sdb_debug    (sdb_t *sdb);


#ifdef __cplusplus
}
#endif
