# sdb

## What is it?

`sdb` is a simple library and data format for serializing data for transmission between computers. It's like json or protocol buffers, but designed with the following goals:

- simple to code, simple to understand, simple to use
- no external dependencies whatsoever
- small code size, and small data size, including the ability to pull data out of the serialized buffer without having to maintain a separate tree structure
- small size and simplicity emphasized over performance

## What languages are supported

Right now there is an implementation for C (and C++ using the C API) and for Python. It should be easy to add languages because the format itself is simple.

## Data format

Any `sdb` buffer starts out with the following two items:

All integer values are little-endian, regardless of machine endianness.

|field|size|description|
|--|--|--|
|id|1B|ID header. Consists of a 3b minor version, a 3b major version, and an machine endianness bit|
|dsize|2B|A `uint16_t` that indicates how many bytes to follow|

Following the header are zero or more data records that look like this:
|field|size|description|
|--|--|--|
|name|n+1 B|A null-terminated string of length n that represent the datum's name
|type|1B|A one byte field indicating the type of the data to follow. Supported types are `int8_t`, `uint8_t`, `int16_t`, `uint16_t`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, and `blob`. Blob indicates just a buffer of bytes|
|size|0 or 2B|most data types do not have this field, but if the type is `blob`, then this field indicates the length of the data to follow|
|data|as indicated by type or size field|0-n B of data. If any of the integer types, this is stored little-endian.


That's it! There is no CRC or other error checking, nor is there an end of file sentinel. It is assumed that correctness of transmission is managed by the transmission layer, so no CRC is present here.

## Example

### First, in C

If you have received a buffer in `sdb` format, you can access the data within like so:
```C
void *buf;
size_t buf_len;
buf_len = recv_from_somewhere(buf,MAX_LEN);

sdb_t mydb;
// error checking not included in this example
int8_t fail = sdb_init(&mydb, buf, buf_len, false);

// now you can read the data:
sdbtypes_t found_type;
uint8_t fdata[8];
int8_t found = sdb_get_int(&sdb, "foo", &found_type, &fdata);
if (found_type == SDB_S16) {
    int16_t foo = (int16_t)fdata;
}
 ```
The maximum size int it 8B, so we allocate a buffer of that size to hold whatever comes. If we are confident that the size will be smaller, we can use a smaller buffer, but this is safest. You can cast over to the size that matches once you know what it is. If the data was not found, then `found` will have been set to an error code.

If you want to read a blob out of an `sdb` buffer, the procedure is basically the same:
```C
const size_t target_size = 512;
uint8_t target[target_size];
size_t actual_size = 0;
int8_t found = sdb_get_blob(&sdb, "bar", target_size, target, &actual_size);
```

If the data will not fit in the provided buffer, and appropriate error code is returned and no data is copied. However, the actual size is set, so you can reallocate your buffer as necessary.

Creating a buffer for transmission is similarly simple:
```C
uint8_t buf[512];
sdb_t osdb;
sdb_init(&osdb,buf,512,true);

int8_t my_s8 = -11;
int8_t sdb_add_int(&sdb, "baz", SDB_S8, &my_s8);
```
When you have added everything you want to add, you can simply get the size of the buffer used and transmit the original buffer you gave to `sdb_init`:

```C
uint16_t buf_used = sdb_size&osdb);
some_sending_function(sdb.buf, buf_used);
```

A few caveats for the C implementation:
- if you do an add of a name already existing in the buffer, with the same type, the value will be updated
- if you do an add of the same name with a different type, you will receive an error message


### Now, in Python

```Python
import sdbuf

mysdb = sdbuf.sdb()
my_bytes = some_fn_that_returns_a_bytes_object()
mysdb.initFromBytes(my_bytes)
data = mysdb.getValues()
```

`data` is a dictionary that contains the decoded data, with a key for each value that will include a `val_bytes` subkey with the raw bytes (useful for blobs) as well as a `value` subkey for decoded integers, and a `type` subkey with the type. Note that python doesn't make a distinction between integer types; `value` will just be a python integer.

Creating an `sdb` buffer is easy, too:

```Python
import sdbuf
mysdb = sdbuf.sdb()
mysdb.set('foo','s32',12345)
mysdb.setBlob('bar',bytes([1,2,3,4,5]))
obuff = mysdb.toBytes()
some_fn_that_sends_data(obuff)
```


#### Author
djacobow (Dave Jacobowitz)

