#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "sdbuf.h"

#define SDB_VS_SZ     (sizeof(sdb_len_t))
#define SDB_HD_SZ     (sizeof(sdb_hdr_t))
#define SDB_HD_OFFSET (0)
#define SDB_VS_OFFSET (SDB_HD_OFFSET+SDB_HD_SZ)
#define SDB_V_OFFSET  (SDB_VS_OFFSET+SDB_VS_SZ)
#define SDB_BLOB_T_SZ (sizeof(sdb_len_t))

// Struct description:
//
// This is psuedocode because real data
// varies with values. We really can't
// use structs easily at all because of 
// unknown alignments...
//
// struct __attribute__((packed)) sdb_datum_t {
//     char name[as_long_as_name];
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
    1,2,4,8,
    1,2,4,8,
    0, 0,
};

static const char *sdbtype_names[] = {
    "s8","s16","s32","s64",
    "u8","u16","u32","u64",
    "blob", "_invalid",
};

int8_t sdb_init(sdb_t *sdb, void *b, const sdb_len_t l, bool clear) {
    sdb->buf = b;
    sdb->len = l;
    if (l < SDB_V_OFFSET) {
        return -SDB_BUFFER_TOO_SMALL;
    }
#pragma GCC diagnostic ignored "-Wuninitialized"
    const sdb_hdr_t vheader = SDB_ID_VAL;
#pragma GCC diagnostic pop
    if (clear) {
        memset(b,0,l);
        memcpy((uint8_t *)b + SDB_HD_OFFSET, &vheader, SDB_HD_SZ);
        sdb->header = vheader;
    } else {
        sdb_hdr_t iheader;
        memcpy(&iheader, (uint8_t *)b + SDB_HD_OFFSET, SDB_HD_SZ);
        if (iheader != vheader) return -SDB_WRONG_VERSION;
    }
    return SDB_OK;
}

static void sdb_get_sizes(sdb_t *sdb) {
    sdb->vals_size = 0;    
    sdb->header = 0;
    memcpy(&sdb->header,(uint8_t *)sdb->buf + SDB_HD_OFFSET,SDB_HD_SZ);
    memcpy(&sdb->vals_size,(uint8_t *)sdb->buf + SDB_VS_OFFSET,SDB_VS_SZ);
}


static void sdb_incr_ptr(void **p, uint32_t incr) {
    uint8_t **as_bytes = (uint8_t **)p;
    *as_bytes += incr;
}

void sdb_debug(sdb_t *sdb) {
    sdb_get_sizes(sdb);
    printf("-d- sdb DEBUG\n");
    printf("-d- ---------\n");
    uint32_t total_size = sdb->vals_size + SDB_V_OFFSET;
    printf("-d- Size: values %u, total %u\n",
        sdb->vals_size, total_size);
    printf("-d- ---------\n");
    void *p = (uint8_t *)sdb->buf + SDB_V_OFFSET;
    sdb_len_t total_dsize = 0;
    while ((p < (void *)((uint8_t *)sdb->buf + sdb->len)) && 
           (p < (void *)((uint8_t *)sdb->buf + SDB_V_OFFSET + sdb->vals_size))) {
        uint16_t valnamelen = strnlen((char *)p,MAX_NAME_LEN+1);
        if (valnamelen > MAX_NAME_LEN) {
            printf("-e- name too long. aborting %s\n",(char *)p);
            return;
        }
        const char *name = (char *)p;
        sdb_incr_ptr(&p,valnamelen+1);
        sdbtypes_t type = _SDB_INVALID_TYPE;
        memcpy(&type,p,sizeof(sdbtypes_t));
        sdb_incr_ptr(&p,sizeof(sdbtypes_t));
        sdb_len_t dsize = 0;
        uint64_t d = 0;
        if (type == SDB_BLOB) {
            memcpy(&dsize, p, SDB_BLOB_T_SZ);
            sdb_incr_ptr(&p,SDB_BLOB_T_SZ);
        } else {
            dsize = sdbtype_sizes[type];
            memcpy(&d, p, dsize);
        }
        total_dsize += dsize;
        sdb_incr_ptr(&p,dsize);

        char nstr[30];
        memset(nstr,0,30);
        switch (type) {
            case SDB_S8:  sprintf(nstr,"%d",(int8_t)d); break;
            case SDB_S16: sprintf(nstr,"%d",(int16_t)d); break;
            case SDB_S32: sprintf(nstr,"%" PRIi32,(int32_t)d); break;
            case SDB_S64: sprintf(nstr,"%" PRIi64,(int64_t)d); break;
            case SDB_U8:  sprintf(nstr,"%u",(uint8_t)d); break;
            case SDB_U16: sprintf(nstr,"%u",(uint16_t)d); break;
            case SDB_U32: sprintf(nstr,"%" PRIu32,(uint32_t)d); break;
            case SDB_U64: sprintf(nstr,"%" PRIu64,(uint64_t)d); break;
            case SDB_BLOB: sprintf(nstr, "%u bytes",dsize); break;
            default: break;
        }                 
        printf("-d- %-16s: %4s : %-20s : 0x%" PRIx64"\n",name, sdbtype_names[type], nstr, type == SDB_BLOB ? dsize : d);
    }
    printf("-d- ---------\n");
    uint32_t overhead_pct = (100 * total_size) / (total_dsize);
    overhead_pct -= 100;
    printf("-d- packed struct would have been: %u bytes. %u%% overhead\n",
        total_dsize, overhead_pct);

};

static void *sdb_find_internal(sdb_t *sdb, const char *name, sdbtypes_t *ftype, sdb_len_t *dsize) {
    sdb_get_sizes(sdb);
    void *p = (uint8_t *)sdb->buf + SDB_V_OFFSET;
    void *pfound = 0;
    while (!pfound && ((uint8_t *)p < (uint8_t *)sdb->buf + SDB_V_OFFSET + sdb->vals_size)) {
        uint16_t vallen = strnlen((char *)p,MAX_NAME_LEN+1);
        if (vallen > MAX_NAME_LEN) {
            return NULL; // should not happen
        }
        if (!strcmp((char *)p,name)) {
            pfound = p;
        }
        sdb_incr_ptr(&p, vallen + 1);
        memcpy(ftype,p,sizeof(sdbtypes_t));
        sdb_incr_ptr(&p, sizeof(sdbtypes_t));
        if (*ftype == SDB_BLOB) {
            sdb_len_t blob_size;
            memcpy(&blob_size,p,SDB_BLOB_T_SZ);
            sdb_incr_ptr(&p,SDB_BLOB_T_SZ);
            sdb_incr_ptr(&p,blob_size);
            *dsize = blob_size;
        } else {
            *dsize = sdbtype_sizes[*ftype];
            sdb_incr_ptr(&p,sdbtype_sizes[*ftype]);
        }
        if (pfound) {
            return pfound;
        }
    }
    return NULL;
}

int8_t sdb_get_int(sdb_t *sdb, const char *name, sdbtypes_t *type, void *data) {
    sdbtypes_t ftype;
    sdb_len_t fsize;
    void *p = sdb_find_internal(sdb, name, &ftype, &fsize); 

    if (p) {
        if (ftype == SDB_BLOB) {
            return -SDB_DIFFERENT_TYPE;
        }
        uint16_t vallen = strnlen((char *)p,MAX_NAME_LEN+1);
        sdb_incr_ptr(&p,vallen + 1);
        memcpy(type,p,sizeof(sdbtypes_t));
        sdb_incr_ptr(&p,sizeof(sdbtypes_t));
        sdb_len_t dsize = sdbtype_sizes[*type];
        memcpy(data,p,dsize);
        sdb_incr_ptr(&p,dsize);
        return SDB_OK;
    }
    return -SDB_NOT_FOUND;
}


int8_t sdb_get_blob (sdb_t *sdb, const char *name, const sdb_len_t omax, void *ob, sdb_len_t *olen) {
    sdbtypes_t ftype;
    sdb_len_t fsize;
    void *p = sdb_find_internal(sdb, name, &ftype, &fsize);
    if (p) {
        if (ftype != SDB_BLOB) {
            return -SDB_DIFFERENT_TYPE;
        }
        uint16_t namelen = strnlen((const char *)p,MAX_NAME_LEN+1);
        sdb_incr_ptr(&p,(namelen+1));
        sdb_incr_ptr(&p,sizeof(sdbtypes_t));
        sdb_len_t dsize;
        memcpy(&dsize,p,SDB_BLOB_T_SZ);
        sdb_incr_ptr(&p,SDB_BLOB_T_SZ);
        *olen = dsize; // set olen even if does not fit
        if (dsize > omax) {
            return -SDB_BUFFER_TOO_SMALL;
        }
        memcpy(ob,p,dsize);
        return SDB_OK;
    }
    return -SDB_NOT_FOUND;
}

int8_t sdb_add_blob (sdb_t *sdb, const char *name, const void *ib, const sdb_len_t ilen) {
    sdbtypes_t ftype;
    sdb_len_t fsize;
    void *pfound = sdb_find_internal(sdb,name,&ftype,&fsize);

    if (pfound) {
        if (ftype != SDB_BLOB) {
            return -SDB_DIFFERENT_TYPE;
        }
        if (fsize != ilen) {
            return -SDB_DIFFERENT_SIZE;
        }
    }

    void *ptarget = 0;
    if (!pfound) {
        ptarget = (uint8_t *)sdb->buf + SDB_V_OFFSET + sdb->vals_size;
        uint16_t namelen = strnlen(name,MAX_NAME_LEN+1);
        if (namelen > MAX_NAME_LEN) {
            return -SDB_NAME_TOO_LONG;
        }
        sdb_len_t bytes_reqd = (namelen+1) + SDB_BLOB_T_SZ + ilen;
        sdb_len_t bytes_avail = sdb->len - sdb->vals_size - SDB_V_OFFSET;
        if (bytes_avail < bytes_reqd) {
            return -SDB_BUFFER_TOO_SMALL;
        }
    } else {
        ptarget = pfound;
    }

    uint16_t namelen = strnlen(name,MAX_NAME_LEN+1);
    strcpy((char *)ptarget,name);
    sdb_incr_ptr(&ptarget,namelen + 1);
    const sdbtypes_t type = SDB_BLOB;
    memcpy(ptarget,&type,sizeof(type));
    sdb_incr_ptr(&ptarget,sizeof(type));
    memcpy(ptarget,&ilen,SDB_BLOB_T_SZ);
    sdb_incr_ptr(&ptarget,SDB_BLOB_T_SZ);
    memcpy(ptarget,ib,ilen);
    sdb_incr_ptr(&ptarget,ilen);
    if (!pfound) {
        sdb->vals_size += (namelen+1) + sizeof(type) + SDB_BLOB_T_SZ + ilen;
        memcpy((uint8_t *)sdb->buf + SDB_VS_OFFSET, &sdb->vals_size, SDB_VS_SZ);
    }
    return SDB_OK;
}


int8_t sdb_add_int(sdb_t *sdb, const char *name, const sdbtypes_t type, const void *data) {
    sdbtypes_t ftype;
    sdb_len_t fsize;
    void *pfound = sdb_find_internal(sdb,name,&ftype,&fsize);

    if (pfound) {
        if (ftype != type) {
            // can't rewrite value if type not the same
            return -SDB_DIFFERENT_TYPE;
        }
    }

    void *ptarget = 0;
    if (!pfound) {
        ptarget = (void *)((uint8_t *)sdb->buf + SDB_V_OFFSET + sdb->vals_size);
        uint16_t namelen = strnlen(name,MAX_NAME_LEN+1);
        if (namelen > MAX_NAME_LEN) {
            return -SDB_NAME_TOO_LONG;
        }
        sdb_len_t dsize = sdbtype_sizes[type];
        sdb_len_t bytes_needed = (namelen+1) + sizeof(type) + dsize;
        sdb_len_t bytes_avail  = sdb->len - sdb->vals_size - SDB_V_OFFSET;
        if (bytes_avail < bytes_needed) {
            return -SDB_BUFFER_TOO_SMALL;
        }
    } else {
        ptarget = pfound;
    }

    uint16_t namelen = strnlen(name,MAX_NAME_LEN+1);
    strcpy((char *)ptarget,name);
    sdb_incr_ptr(&ptarget,namelen + 1);
    memcpy(ptarget,&type,sizeof(type));
    sdb_incr_ptr(&ptarget,sizeof(type));
    sdb_len_t dsize = sdbtype_sizes[type];
    memcpy(ptarget, data, dsize);
    if (!pfound) {
        sdb->vals_size += (namelen+1) + sizeof(type) + dsize;
        memcpy((uint8_t *)sdb->buf + SDB_VS_OFFSET, &sdb->vals_size, SDB_VS_SZ);
    }
    return SDB_OK;
}


sdb_len_t sdb_size(sdb_t *sdb) {
    sdb_get_sizes(sdb);
    return SDB_V_OFFSET + sdb->vals_size;
}
