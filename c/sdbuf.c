#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "sdbuf.h"

#define SDB_LEN_SZ       (sizeof(sdb_len_t))
#define SDB_TLEN_SZ      (sizeof(sdb_tlen_t))
#define SDB_HDR_SZ       (sizeof(sdb_hdr_t))
#define SDB_HDR_OFFSET   (0)
#define SDB_TLEN_OFFSET  (SDB_HDR_OFFSET+SDB_HDR_SZ)
#define SDB_VALS_OFFSET  (SDB_TLEN_OFFSET+SDB_TLEN_SZ)
#define SDB_BLOB_T_SZ    (sizeof(sdb_len_t))
#define SDB_COUNT_T_SZ   (sizeof(sdb_len_t))
#define SDB_ARRAY_T_FLAG (0x80)
#define SDB_ID_SZ        (sizeof(sdb_id_t))

// Struct description:
//
// This is psuedocode because real data
// varies with values. We really can't
// use structs easily at all because of 
// unknown alignments...
//
// struct __attribute__((packed)) sdb_datum_t {
//     sdb_id_t id;
//     sdbtypes_t type; 
//     sdb_len_t blob_length; (or nothing if not blob)
//     uint8_t data[as_long_as_data]
// }
//
// struct __attribute__((packed)) {
//     sdb_hdr_t header;
//     sdb_len_t data_size;
//     sdb_datum_t[num_of_data];   
// }

// Usage:
//
// If creating a buffer with new data:
//
// 1. call sdb_init with a suitable buffer, and clear as true,
//    so that the buffer is cleared
// 2. call sdb_add or sdb_add_blob as many times as required
// 3. call sdb_size to see how much buffer is used
//
// If starting with a received buffer (with data):
//
// 1. call sdb_init with the buffer and length, anad clear as false
// 2. call sdb_get as required

// #define IS_BIG_ENDIAN (!*(unsigned char *)&(uint16_t){1})
#define IS_BIG_ENDIAN (0)

#define SDB_ID_VAL (((SDB_VER_MAJOR & 0x7) << 3) | \
                    ((SDB_VER_MINOR & 0x7) << 0) | \
                    ((IS_BIG_ENDIAN ? 0x80 : 0x0)))

static const uint8_t sdbtype_sizes[] = {
    sizeof(int8_t), sizeof(int16_t), sizeof(int32_t), sizeof(int64_t),
    sizeof(uint8_t), sizeof(uint16_t), sizeof(uint32_t), sizeof(uint64_t),
    sizeof(float), sizeof(double),
    0, 0,
};

static const char *sdbtype_names[] = {
    "s8","s16","s32","s64",
    "u8","u16","u32","u64",
    "float", "double",
    "blob", "_invalid",
};

static void sdb_rewrite_sizes(sdb_t *sdb) {
    sdb->vals_size = 0;    
    sdb->header = 0;
    memcpy(&sdb->header,(uint8_t *)sdb->buf + SDB_HDR_OFFSET, SDB_HDR_SZ);
    memcpy(&sdb->vals_size,(uint8_t *)sdb->buf + SDB_TLEN_OFFSET, SDB_TLEN_SZ);
}

int8_t sdb_init(sdb_t *sdb, void *b, const sdb_tlen_t l, bool clear) {
    sdb->buf = b;
    sdb->len = l;
    if (l < SDB_VALS_OFFSET) {
        return -SDB_BUFFER_TOO_SMALL;
    }
    const sdb_hdr_t vheader = SDB_ID_VAL;
    if (clear) {
        memset(b,0,l);
        memcpy((uint8_t *)b + SDB_HDR_OFFSET, &vheader, SDB_HDR_SZ);
        sdb->header = vheader;
    } else {
        sdb_hdr_t iheader;
        memcpy(&iheader, (uint8_t *)b + SDB_HDR_OFFSET, SDB_HDR_SZ);
        // only check major and endianness
        if ((iheader & 0xf8) != (vheader & 0xf8)) return -SDB_WRONG_VERSION;
    }
    sdb_rewrite_sizes(sdb);
    return SDB_OK;
}

void sdb_debug(sdb_t *sdb) {
    printf("-d- sdb DEBUG\n");
    printf("-d- ---------\n");
    uint32_t total_size = sdb->vals_size + SDB_VALS_OFFSET;
    printf("-d- Size: values %u, total %u\n",
        sdb->vals_size, total_size);
    printf("-d- ---------\n");
    uint8_t *p = (uint8_t *)sdb->buf + SDB_VALS_OFFSET;
    sdb_len_t total_dsize = 0;
    while ((p < ((uint8_t *)sdb->buf + sdb->len)) && 
           (p < ((uint8_t *)sdb->buf + SDB_VALS_OFFSET + sdb->vals_size))) {
        sdb_id_t id = 0;
        memcpy(&id, p, SDB_ID_SZ);
        p += SDB_ID_SZ;
        sdbtypes_t type = _SDB_INVALID_TYPE;
        memcpy(&type,p,sizeof(sdbtypes_t));
        uint8_t is_array = type & SDB_ARRAY_T_FLAG;
        type &= ~SDB_ARRAY_T_FLAG;
        p += sizeof(sdbtypes_t);
        sdb_len_t count = 1;
        sdb_len_t dsize = 0;
        sdb_val_t d;
        d.u64 = 0;
        if (type == SDB_BLOB) {
            memcpy(&dsize, p, SDB_BLOB_T_SZ);
            p += SDB_BLOB_T_SZ;
        } else {
            dsize = sdbtype_sizes[type];
        }
        if (is_array) {
            memcpy(&count,p,SDB_COUNT_T_SZ);
            p += SDB_COUNT_T_SZ;
        };
        total_dsize += count * dsize;

        for (sdb_len_t i=0; i<count; i++) {
            sdb_val_t d = {};
            if (type != SDB_BLOB) {
                memcpy(&d, p, dsize);
            }
            p += dsize;
            char nstr[30];
            memset(nstr,0,30);
            switch (type) {
                case SDB_S8:     sprintf(nstr,"%d",d.s8); break;
                case SDB_S16:    sprintf(nstr,"%d",d.s16); break;
                case SDB_S32:    sprintf(nstr,"%" PRIi32,d.s32); break;
                case SDB_S64:    sprintf(nstr,"%" PRIi64,d.s64); break;
                case SDB_U8:     sprintf(nstr,"%u",d.u8); break;
                case SDB_U16:    sprintf(nstr,"%u",d.u16); break;
                case SDB_U32:    sprintf(nstr,"%" PRIu32,d.u32); break;
                case SDB_U64:    sprintf(nstr,"%" PRIu64,d.u64); break;
#if SDB_INCL_FLOAT
                case SDB_FLOAT:  sprintf(nstr,"%f",d.f); break;
                case SDB_DOUBLE: sprintf(nstr,"%f",d.d); break;
#endif
                case SDB_BLOB:   sprintf(nstr,"%u bytes",dsize); break;
                default: break;
            }                 
            printf("-d- %04x: %4s : %u/%u : %-20s : 0x%" PRIx64"\n",id, sdbtype_names[type], i, count, nstr, type == SDB_BLOB ? dsize : d.u64);
        } 
    }
    printf("-d- ---------\n");
    uint32_t overhead_pct = (100 * total_size) / (total_dsize);
    overhead_pct -= 100;
    printf("-d- packed struct would have been: %u bytes. %u%% overhead\n",
        total_dsize, overhead_pct);

};


static uint8_t *sdb_find_internal(const sdb_t *sdb, sdb_id_t id, sdbtypes_t *ftype, sdb_len_t *dsize, sdb_len_t *dcount, uint8_t **next) {
    uint8_t *p = (uint8_t *)sdb->buf + SDB_VALS_OFFSET;
    uint8_t *pfound = 0;
    while (!pfound && ((uint8_t *)p < (uint8_t *)sdb->buf + SDB_VALS_OFFSET + sdb->vals_size)) {
        sdb_id_t rec_id = 0;
        memcpy(&rec_id, p, SDB_ID_SZ);
        if (rec_id == id) {
            pfound = p;
        }
        p += SDB_ID_SZ;
        memcpy(ftype,p,sizeof(sdbtypes_t));
        p += sizeof(sdbtypes_t);
        uint8_t is_array = (*ftype & SDB_ARRAY_T_FLAG);
        *ftype &= ~SDB_ARRAY_T_FLAG;
        
        *dcount = 1;
        if (*ftype == SDB_BLOB) {
            memcpy(dsize,p,SDB_BLOB_T_SZ);
            p += SDB_BLOB_T_SZ;
        } else {
            *dsize = sdbtype_sizes[*ftype];
        }
        if (is_array) {
            memcpy(dcount,p,SDB_COUNT_T_SZ);
            p += SDB_COUNT_T_SZ;
        }
        p += *dcount * *dsize;

        if (pfound) {
            *next = p;
            return pfound;
        }
    }
    *next = p;
    return NULL;
}

int8_t sdb_find(const sdb_t *sdb, sdb_id_t id, sdb_member_info_t *about) {
    sdbtypes_t ftype;
    sdb_len_t dsize;
    sdb_len_t dcount;
    uint8_t *next;
    uint8_t *p = sdb_find_internal(sdb, id, &ftype, &dsize, &dcount, &next);
    if (p) {
        about->handle = p;
        memcpy((void *)&about->id, &id, SDB_ID_SZ);
        about->type = ftype;
        about->elemsize = dsize;
        about->elemcount = dcount;
        about->minsize = dsize * dcount;
        return SDB_OK;
    }
    about->handle = NULL;
    about->id = 0;
    about->type = _SDB_INVALID_TYPE;
    about->elemsize = 0;
    about->elemcount = 0;
    about->minsize = 0;
    return -SDB_NOT_FOUND;
}

int8_t sdb_get(const sdb_member_info_t *abt, void *data) {
    if (!abt)  return -SDB_BAD_HANDLE;
    uint8_t *p = abt->handle;
    if (!p) return -SDB_BAD_HANDLE;

    p += SDB_ID_SZ;

    sdbtypes_t type = _SDB_INVALID_TYPE;
    sdb_len_t dsize = 0;
    memcpy(&type,p,sizeof(sdbtypes_t));
    uint8_t is_array = type & SDB_ARRAY_T_FLAG;
    type &= ~SDB_ARRAY_T_FLAG;
    p += sizeof(sdbtypes_t);
    if (type != abt->type) return -SDB_DIFFERENT_TYPE;
    if (type == SDB_BLOB) {
        memcpy(&dsize,p,SDB_BLOB_T_SZ);
        p += SDB_BLOB_T_SZ;
    } else {
        dsize = sdbtype_sizes[type];
    }
    if (dsize != abt->elemsize) return -SDB_DIFFERENT_SIZE;

    sdb_len_t dcount = 1;
    if (is_array) {
        memcpy(&dcount,p,SDB_COUNT_T_SZ);
        p += SDB_COUNT_T_SZ;
    }
    if (dcount != abt->elemcount) return -SDB_DIFFERENT_COUNT;

    memcpy(data, p, dsize * dcount);
    return SDB_OK;
}

static int8_t sdb_remove_internal(sdb_t *sdb, uint8_t *pelem, uint8_t *pnext) {
    if (pelem) {
        if (pnext > pelem) {
            uint8_t *pvals = (uint8_t *)sdb->buf + SDB_VALS_OFFSET;
            size_t elem_size = pnext - pelem;
            uint8_t *pend = pvals + sdb->vals_size;
            size_t rem_len = pend - pnext;
            memmove(pelem, pnext, rem_len);
            sdb->vals_size -= elem_size;
            memcpy((uint8_t *)sdb->buf + SDB_TLEN_OFFSET, &sdb->vals_size, SDB_LEN_SZ);
            sdb_rewrite_sizes(sdb);
            return SDB_OK;
        } else {
            return -SDB_SCAN_ERROR;
        } 
    }
    return -SDB_NOT_FOUND;
}

int8_t sdb_add_blob (sdb_t *sdb, sdb_id_t id, const void *ib, const sdb_len_t ilen) {
    sdbtypes_t ftype;
    sdb_len_t fsize;
    sdb_len_t fcount;
    uint8_t *next;
    uint8_t *pfound = sdb_find_internal(sdb, id, &ftype, &fsize, &fcount, &next);

    if (pfound) {
        sdb_remove_internal(sdb, pfound, next);
    }

    uint8_t *ptarget = (uint8_t *)sdb->buf + SDB_VALS_OFFSET + sdb->vals_size;
    sdb_len_t bytes_reqd = SDB_ID_SZ + SDB_BLOB_T_SZ + ilen;
    sdb_len_t bytes_avail = sdb->len - sdb->vals_size - SDB_VALS_OFFSET;
    if (bytes_avail < bytes_reqd) {
        return -SDB_BUFFER_TOO_SMALL;
    }

    memcpy((void*)ptarget, &id, SDB_ID_SZ);
    ptarget += SDB_ID_SZ;
    const sdbtypes_t type = SDB_BLOB;
    memcpy(ptarget,&type,sizeof(type));
    ptarget += sizeof(type);
    memcpy(ptarget,&ilen,SDB_BLOB_T_SZ);
    ptarget += SDB_BLOB_T_SZ;
    memcpy(ptarget,ib,ilen);
    ptarget += ilen;

    sdb->vals_size += SDB_ID_SZ + sizeof(type) + SDB_BLOB_T_SZ + ilen;
    memcpy((uint8_t *)sdb->buf + SDB_TLEN_OFFSET, &sdb->vals_size, SDB_LEN_SZ);
    sdb_rewrite_sizes(sdb);
    return SDB_OK;
}




int8_t sdb_remove(sdb_t *sdb, sdb_id_t id) {
    sdbtypes_t  ftype;
    sdb_len_t   dsize;
    sdb_len_t   dcount;
    uint8_t    *next;
    uint8_t *p = sdb_find_internal(sdb, id, &ftype, &dsize, &dcount, &next);
    if (p) {
        return sdb_remove_internal(sdb, p, next);
    }
    return -SDB_NOT_FOUND;
}

int8_t sdb_set_val(sdb_t *sdb, sdb_id_t id, const sdbtypes_t type, const void *data) {
    return sdb_set_vala(sdb, id,type, 1, data);
}

int8_t sdb_set_vala(sdb_t *sdb, sdb_id_t id, const sdbtypes_t type, const sdb_len_t count, const void *data) {
    sdbtypes_t ftype;
    sdb_len_t fsize;
    sdb_len_t fcount;
    uint8_t *next;
    uint8_t *pfound = sdb_find_internal(sdb, id, &ftype, &fsize, &fcount, &next);
    uint8_t is_array = count != 1;

    if (pfound) {
        sdb_remove_internal(sdb, pfound, next);
    }

    uint8_t *ptarget = ((uint8_t *)sdb->buf + SDB_VALS_OFFSET + sdb->vals_size);
    sdb_len_t dsize = sdbtype_sizes[type];
    sdb_len_t bytes_needed = SDB_ID_SZ + sizeof(type) + count * dsize;
    if (is_array) bytes_needed += SDB_COUNT_T_SZ;
    sdb_len_t bytes_avail  = sdb->len - sdb->vals_size - SDB_VALS_OFFSET;
    if (bytes_avail < bytes_needed) {
        return -SDB_BUFFER_TOO_SMALL;
    }

    memcpy((void *)ptarget, &id, SDB_ID_SZ);
    ptarget += SDB_ID_SZ;
    sdbtypes_t stype = type;
    if (is_array) {
        stype |= SDB_ARRAY_T_FLAG;
    }
    memcpy(ptarget,&stype,sizeof(type));
    ptarget += sizeof(type);
    if (is_array) {
        memcpy(ptarget,&count,SDB_COUNT_T_SZ);
        ptarget += SDB_COUNT_T_SZ;
    }
    memcpy(ptarget, data, count * dsize);

    sdb->vals_size += SDB_ID_SZ + sizeof(type) + count * dsize;
    if (is_array) sdb->vals_size += SDB_COUNT_T_SZ;
    memcpy((uint8_t *)sdb->buf + SDB_TLEN_OFFSET, &sdb->vals_size, SDB_LEN_SZ);
    sdb_rewrite_sizes(sdb);
    return SDB_OK;
}


sdb_len_t sdb_size(const sdb_t *sdb) {
    return SDB_VALS_OFFSET + sdb->vals_size;
}


int8_t sdb_set_unsigned(sdb_t *sdb, sdb_id_t id, uint64_t uv) {
    sdb_val_t v;
    if (uv <= UINT8_MAX) {
       v.u8 = uv;
       return sdb_set_val(sdb, id, SDB_U8, &v);
    }
    if (uv <= UINT16_MAX) {
       v.u16 = uv;
       return sdb_set_val(sdb, id, SDB_U16, &v);
    }
    if (uv <= UINT32_MAX) {
       v.u32 = uv;
       return sdb_set_val(sdb, id, SDB_U32, &v);
    }
    v.u64 = uv;
    return sdb_set_val(sdb, id, SDB_U64, &v);
}

int8_t sdb_set_signed(sdb_t *sdb, sdb_id_t id, int64_t iv) {
    sdb_val_t v;
    if ((iv >= INT8_MIN) && (iv <= INT8_MAX)) {
       v.s8 = iv;
       return sdb_set_val(sdb, id, SDB_S8, &v);
    }
    if ((iv >= INT16_MIN) && (iv <= INT16_MAX)) {
       v.s16 = iv;
       return sdb_set_val(sdb, id, SDB_S16, &v);
    }
    if ((iv >= INT32_MIN) && (iv <= INT32_MAX)) {
       v.s32 = iv;
       return sdb_set_val(sdb, id, SDB_S32, &v);
    }
    v.s64 = iv;
    return sdb_set_val(sdb, id, SDB_S64, &v);
}


uint64_t sdb_get_unsigned(const sdb_t *sdb, sdb_id_t id, int8_t *error) {
    sdb_member_info_t about;
    uint64_t rv = 0;
    sdb_val_t v =  {};

    int8_t err = sdb_find(sdb, id, &about);

    if (err == SDB_OK) {
        err = sdb_get(&about, &v);
        if (err == SDB_OK) {
            switch (about.type) {
                case SDB_U8:  rv = v.u8;  break;
                case SDB_U16: rv = v.u16; break;
                case SDB_U32: rv = v.u32; break;
                case SDB_U64: rv = v.u64; break;
                default:
                    err = -SDB_DIFFERENT_TYPE;
            }
        }
    }

    if (error) {
        *error = err;
    } 
    return rv;
}

int64_t sdb_get_signed(const sdb_t *sdb, sdb_id_t id, int8_t *error) {
    sdb_member_info_t about;
    uint64_t rv = 0;
    sdb_val_t v =  {};

    int8_t err = sdb_find(sdb, id, &about);

    if (err == SDB_OK) {
        err = sdb_get(&about, &v);
        if (err == SDB_OK) {
            switch (about.type) {
                case SDB_S8:  rv = v.s8;  break;
                case SDB_S16: rv = v.s16; break;
                case SDB_S32: rv = v.s32; break;
                case SDB_S64: rv = v.s64; break;
                default:
                    err = -SDB_DIFFERENT_TYPE;
            }
        }
    }

    if (error) {
        *error = err;
    } 
    return rv;
}
