#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "sdbuf.h"

#define SDB_VS_SZ      (sizeof(sdb_len_t))
#define SDB_HD_SZ      (sizeof(sdb_hdr_t))
#define SDB_HD_OFFSET  (0)
#define SDB_VS_OFFSET  (SDB_HD_OFFSET+SDB_HD_SZ)
#define SDB_V_OFFSET   (SDB_VS_OFFSET+SDB_VS_SZ)
#define SDB_BLOB_T_SZ  (sizeof(sdb_len_t))
#define SDB_COUNT_T_SZ (sizeof(sdb_len_t))
#define SDB_ARRAY_T_FLAG (0x80)


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

int8_t sdb_init(sdb_t *sdb, void *b, const sdb_len_t l, bool clear) {
    sdb->buf = b;
    sdb->len = l;
    if (l < SDB_V_OFFSET) {
        return -SDB_BUFFER_TOO_SMALL;
    }
    const sdb_hdr_t vheader = SDB_ID_VAL;
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


void sdb_debug(sdb_t *sdb) {
    sdb_get_sizes(sdb);
    printf("-d- sdb DEBUG\n");
    printf("-d- ---------\n");
    uint32_t total_size = sdb->vals_size + SDB_V_OFFSET;
    printf("-d- Size: values %u, total %u\n",
        sdb->vals_size, total_size);
    printf("-d- ---------\n");
    uint8_t *p = (uint8_t *)sdb->buf + SDB_V_OFFSET;
    sdb_len_t total_dsize = 0;
    while ((p < ((uint8_t *)sdb->buf + sdb->len)) && 
           (p < ((uint8_t *)sdb->buf + SDB_V_OFFSET + sdb->vals_size))) {
        uint16_t valnamelen = strnlen((char *)p,MAX_NAME_LEN+1);
        if (valnamelen > MAX_NAME_LEN) {
            printf("-e- name too long. aborting %s\n",(char *)p);
            return;
        }
        const char *name = (char *)p;
        p += valnamelen + 1;
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
                case SDB_FLOAT:  sprintf(nstr,"%f",d.f); break;
                case SDB_DOUBLE: sprintf(nstr,"%f",d.d); break;
                case SDB_BLOB:   sprintf(nstr, "%u bytes",dsize); break;
                default: break;
            }                 
            printf("-d- %-16s: %4s : %u/%u : %-20s : 0x%" PRIx64"\n",name, sdbtype_names[type], i, count, nstr, type == SDB_BLOB ? dsize : d.u64);
        } 
    }
    printf("-d- ---------\n");
    uint32_t overhead_pct = (100 * total_size) / (total_dsize);
    overhead_pct -= 100;
    printf("-d- packed struct would have been: %u bytes. %u%% overhead\n",
        total_dsize, overhead_pct);

};

static uint8_t *sdb_find_internal(sdb_t *sdb, const char *name, sdbtypes_t *ftype, sdb_len_t *dsize, sdb_len_t *dcount) {
    sdb_get_sizes(sdb);
    uint8_t *p = (uint8_t *)sdb->buf + SDB_V_OFFSET;
    uint8_t *pfound = 0;
    while (!pfound && ((uint8_t *)p < (uint8_t *)sdb->buf + SDB_V_OFFSET + sdb->vals_size)) {
        uint16_t vallen = strnlen((char *)p,MAX_NAME_LEN+1);
        if (vallen > MAX_NAME_LEN) {
            return NULL; // should not happen
        }
        if (!strcmp((char *)p,name)) {
            pfound = p;
        }
        p += vallen + 1;
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
            return pfound;
        }
    }
    return NULL;
}

int8_t sdb_find(sdb_t *sdb, const char *name, sdb_member_info_t *about) {
    sdbtypes_t ftype;
    sdb_len_t dsize;
    sdb_len_t dcount;
    uint8_t *p = sdb_find_internal(sdb, name, &ftype, &dsize, &dcount);
    if (p) {
        about->handle = p;
        about->name = (const char *)p;
        about->type = ftype;
        about->elemsize = dsize;
        about->elemcount = dcount;
        about->minsize = dsize * dcount;
        return SDB_OK;
    }
    about->handle = NULL;
    about->name = NULL;
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

    uint16_t vallen = strnlen((char *)p,MAX_NAME_LEN+1);
    p += vallen + 1;

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

int8_t sdb_add_blob (sdb_t *sdb, const char *name, const void *ib, const sdb_len_t ilen) {
    sdbtypes_t ftype;
    sdb_len_t fsize;
    sdb_len_t fcount;
    char *fname = NULL;
    uint8_t *pfound = sdb_find_internal(sdb,name,&ftype,&fsize,&fcount);

    if (pfound) {
        if (ftype != SDB_BLOB) {
            return -SDB_DIFFERENT_TYPE;
        }
        if (fsize != ilen) {
            return -SDB_DIFFERENT_SIZE;
        }
    }

    uint8_t *ptarget = 0;
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
    ptarget += namelen + 1;
    const sdbtypes_t type = SDB_BLOB;
    memcpy(ptarget,&type,sizeof(type));
    ptarget += sizeof(type);
    memcpy(ptarget,&ilen,SDB_BLOB_T_SZ);
    ptarget += SDB_BLOB_T_SZ;
    memcpy(ptarget,ib,ilen);
    ptarget += ilen;
    if (!pfound) {
        sdb->vals_size += (namelen+1) + sizeof(type) + SDB_BLOB_T_SZ + ilen;
        memcpy((uint8_t *)sdb->buf + SDB_VS_OFFSET, &sdb->vals_size, SDB_VS_SZ);
    }
    return SDB_OK;
}


int8_t sdb_add_val(sdb_t *sdb, const char *name, const sdbtypes_t type, const void *data) {
    return sdb_add_vala(sdb,name,type,1,data);
}

int8_t sdb_add_vala(sdb_t *sdb, const char *name, const sdbtypes_t type, const sdb_len_t count, const void *data) {
    sdbtypes_t ftype;
    sdb_len_t fsize;
    sdb_len_t fcount;
    uint8_t *pfound = sdb_find_internal(sdb,name,&ftype,&fsize,&fcount);
    uint8_t is_array = count != 1;

    if (pfound) {
        if (ftype != type) {
            // can't rewrite value if type not the same
            return -SDB_DIFFERENT_TYPE;
        }
        if (fcount != count) {
            return -SDB_DIFFERENT_COUNT;
        }
    }

    uint8_t *ptarget = 0;
    if (!pfound) {
        ptarget = ((uint8_t *)sdb->buf + SDB_V_OFFSET + sdb->vals_size);
        uint16_t namelen = strnlen(name,MAX_NAME_LEN+1);
        if (namelen > MAX_NAME_LEN) {
            return -SDB_NAME_TOO_LONG;
        }
        sdb_len_t dsize = sdbtype_sizes[type];
        sdb_len_t bytes_needed = (namelen+1) + sizeof(type) + count * dsize;
        if (is_array) bytes_needed += SDB_COUNT_T_SZ;
        sdb_len_t bytes_avail  = sdb->len - sdb->vals_size - SDB_V_OFFSET;
        if (bytes_avail < bytes_needed) {
            return -SDB_BUFFER_TOO_SMALL;
        }
    } else {
        ptarget = pfound;
    }

    uint16_t namelen = strnlen(name,MAX_NAME_LEN+1);
    strcpy((char *)ptarget,name);
    ptarget += namelen + 1;
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
    sdb_len_t dsize = sdbtype_sizes[type];
    memcpy(ptarget, data, count * dsize);
    if (!pfound) {
        sdb->vals_size += (namelen+1) + sizeof(type) + count * dsize;
        if (is_array) sdb->vals_size += SDB_COUNT_T_SZ;
        memcpy((uint8_t *)sdb->buf + SDB_VS_OFFSET, &sdb->vals_size, SDB_VS_SZ);
    }
    return SDB_OK;
}


sdb_len_t sdb_size(sdb_t *sdb) {
    sdb_get_sizes(sdb);
    return SDB_V_OFFSET + sdb->vals_size;
}
