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
// scalars 
typedef union sdb_val_t {
    uint8_t  u8;
    int8_t   s8;
    uint16_t u16;
    int16_t  s16;
    uint32_t u32;
    int32_t  s32;
    uint64_t u64;
    int64_t  s64;
    float    f;
    double   d;
} sdb_val_t;

typedef enum __attribute__((__packed__)) sdbtypes_t {
    SDB_S8, SDB_S16, SDB_S32, SDB_S64,
    SDB_U8, SDB_U16, SDB_U32, SDB_U64,
    SDB_FLOAT, SDB_DOUBLE,
    SDB_BLOB,
    _SDB_INVALID_TYPE,
} sdbtypes_t;

typedef enum sdb_errors_t {
    SDB_OK = 0,
    SDB_BUFFER_TOO_SMALL,
    SDB_DIFFERENT_TYPE,
    SDB_DIFFERENT_COUNT,
    SDB_NOT_FOUND,
    SDB_DIFFERENT_SIZE,
    SDB_NAME_TOO_LONG,
    SDB_WRONG_VERSION,
    SDB_BAD_HANDLE,
} sdb_errors_t;

typedef struct sdb_t {
    void *buf;
    sdb_len_t len;
    sdb_hdr_t header;
    sdb_len_t vals_size;
} sdb_t;

// this structure is set up by sdb_find and contains
// both the info sdb needs to find a data element as
// well as info you need to determine the type and
// size of a found data element
typedef struct sdb_member_info_t {
    uint8_t    *handle;
    const char *name;
    sdbtypes_t type;
    sdb_len_t  elemsize;
    sdb_len_t  elemcount;
    size_t     minsize;
} sdb_member_info_t;

// initialize the struct with the target buffer, optionally zero it out
int8_t   sdb_init     (sdb_t *sdb, void *b, const sdb_len_t l, bool clear);

// obtain total size of blob. Primary use is if you are about to 
// transmit or write out the buffer
sdb_len_t sdb_size    (sdb_t *sdb);

// setters for standard types:
// .. an array
int8_t   sdb_add_vala (sdb_t *sdb, const char *name,
                       const sdbtypes_t type, const sdb_len_t count, const void *data);
// .. or just one
int8_t   sdb_add_val  (sdb_t *sdb, const char *name,
                       const sdbtypes_t type, const void *data);

// setter for blobs
int8_t   sdb_add_blob (sdb_t *sdb, const char *name, 
                       const void *ib, const sdb_len_t isize);

// "find" an item by name and set up a member_info_t with a pointer
// to the object as well as metadata you need to size a receiving
// buffer
int8_t   sdb_find(sdb_t *sdb, const char *name, sdb_member_info_t *about);

// A simple, generic getter. "data" must be large enough to hold
// the data. Inspect the member_info_t.minsize value to determine
// the minimum receiving size.
int8_t   sdb_get      (const sdb_member_info_t *about, void *data);

// debug dumper
void     sdb_debug    (sdb_t *sdb);

#ifdef __cplusplus
}
#endif
