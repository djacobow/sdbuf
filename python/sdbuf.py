#!/usr/bin/env python3

import struct
from sys import byteorder

class SDBException(Exception):
    pass

class sdb:

    constants = {
        'VER_MAJOR':  0x2,
        'VER_MINOR':  0x0,
        'SIZE_SIZE':  2,
        'COUNT_SIZE': 2,
        'TYPE_SIZE':  1,
        'KEY_SIZE':   2,
        'HD_SIZE'  :  1,
        'VS_SIZE'  :  4,
        'HD_OFFSET':  0, 
        'MAX_ID':     0xffff
    }
    constants['VS_OFFSET'] = (
        constants['HD_OFFSET'] +
        constants['HD_SIZE']
    )

    constants['V_OFFSET'] = (
        constants['VS_OFFSET'] +
        constants['VS_SIZE']
    )

    constants['ID_VAL'] = (
        ((constants['VER_MAJOR'] & 0x7) << 3) |
        ((constants['VER_MINOR'] & 0x7) << 0) |
        (0x80 if byteorder == 'bib' else 0x0)
    )
    type_array_flag = 0x80;

    types = {
       's8':      { 'idx': 0,  'size': 1, 'signed': True,   'range': (-128, 127)  }, 
       's16':     { 'idx': 1,  'size': 2, 'signed': True,   'range': (-32768, 32767)  }, 
       's32':     { 'idx': 2,  'size': 4, 'signed': True,   'range': (-2_147_483_648, 2_147_483_647) }, 
       's64':     { 'idx': 3,  'size': 8, 'signed': True,   'range': (-9_223_372_036_854_775_808, 9_223_372_036_854_775_807)}, 
       'u8':      { 'idx': 4,  'size': 1, 'signed': False,  'range': (0, 255) }, 
       'u16':     { 'idx': 5,  'size': 2, 'signed': False,  'range': (0, 65535) }, 
       'u32':     { 'idx': 6,  'size': 4, 'signed': False,  'range': (0, 4_2954_967_295) }, 
       'u64':     { 'idx': 7,  'size': 8, 'signed': False,  'range': (0, 18_446_744_073_709_551_615) }, 
       'float':   { 'idx': 8,  'size': 4 }, 
       'double':  { 'idx': 9,  'size': 8 }, 
       'blob':    { 'idx': 10, 'size': 0 },
       '_inv':    { 'idx': 11, 'size': 0 },
    }

    type_names = [ x for x in types ]

    types_reversed = {}
    for t in types.items():
        if t[1].get('signed') is not None:
            signedness = t[1]['signed']
            size = t[1]['size']
            if signedness not in types_reversed:
                types_reversed[signedness] = {}
            types_reversed[signedness][size] = t[0]



    def __init__(self, input: bytes|bytearray|str|dict|None):
        self.buf = bytearray()

        if input is None:
            pass
        elif isinstance(input, dict):
            self.vals = {}
            self.fromDict(input)
        elif isinstance(input, str):
            try:
                with open(input, "rb") as ifh:
                    self.buf = ifh.read()
            except Exception as e:
                raise SDBException(e)
            self.__scan()
        elif isinstance(input, (bytes, bytearray)):
            self.buf = input;
            self.__scan()
       

    # takes a scalar thing or a list of things of the same type
    def setVal(self, name, typename, value):
        if not isinstance(value,list):
            value = [value]

        if name is None:
            raise SDBException('null id key provided')
        if not isinstance(name,int) and name <= self.constants['MAX_ID']:
            raise SDBException('invalid id key')
        if typename is None:
            raise SDBException('null typename')
        if not typename in self.types:
            raise SDBException(f'unknown typename {typename}')
        self.vals[name] = {
            'type': typename,
            'value': value,
        }

    # takes a bytearray or a list of bytearrays of equal length"
    def setBlob(self,name,vbytes: list|tuple|bytes|bytearray):
        if not isinstance(vbytes,list):
            vbytes = [vbytes]
        self.vals[name] = {
            'type': self.types['blob']['idx'],
            'val_bytes': vbytes,
        }

    def asDictDetailed(self):
        return self.vals

    def asDict(self):
        rv = {}
        for i in self.vals.items():
            is_blob   = i[1]['type'] == 'blob'
            src = 'val_bytes' if is_blob else 'value'
            ov  = i[1][src]
            is_scalar = isinstance(ov,(list,tuple)) and len(ov) <= 1
            if is_scalar:
                ov  = ov[0]
            rv[i[0]] = ov
        return rv

    def fromDict(self, d):
        for i in d.items():
            v = i[1]
            k = i[0]
            t = self.__guessType(v)
            self.setVal(k, t, v)

    def __guessType(self, vs):
        if not isinstance(vs, (list,tuple)):
            vs = [vs]

        is_signed = False
        max_size  = 1
        for v in vs:
            if v < 0:
                is_signed = True
            if isinstance(v, float):
                return 'double'
            if isinstance(v, (bytes, bytearray)):
                return 'blob'

        if not all((isinstance(e,int) for e in vs)):
            raise SDBException(f'Cannot infer type of {vs}')

        for v in vs:
            this_size = 1
            for i in (1,2,4,8):
                t = self.types_reversed[is_signed][i]
                if (v >= self.types[t]['range'][0]) and (v <= self.types[t]['range'][1]):
                    this_size = i
                    break
            if this_size > max_size:
                max_size = this_size

        return self.types_reversed[is_signed][max_size]
             

    def debug(self):
        for name in self.vals:
            val = self.vals[name]
            if val['type'] == 'blob':
                print(" {:17} {:4} val {}".format(
                    name,val['type'],
                    self.__bytesByFour(b''.join(self.vals[name]['val_bytes']))))
            else:
                print(" {:17} {:4} val {:20} {:30}".format(
                    name,
                    val['type'], 
                    ''.join(['[',','.join([str(x) for x in val['value']]),']']),
                    self.__bytesByFour(list(reversed(b''.join(self.vals[name]['val_bytes'])))))
                )

    def saveToFile(self,fn):
        obytes = self.toBytes()
        try:
            with open(fn, 'wb') as ofh:
                ofh.write(obytes)
        except Exception as e:
            raise SDBException(e)

    def toBytes(self):
        self.buf = bytearray()
        self.buf += bytes(self.constants['V_OFFSET'])
        for key in self.vals:
            val = self.vals[key]
            self.buf += struct.pack('H',key)
            dcount = len(val['value'])
            outtype = self.types[val['type']]['idx']
            if dcount != 1:
                outtype |= self.type_array_flag

            self.buf += outtype.to_bytes(
                self.constants['TYPE_SIZE'],
                signed=False,
                byteorder='little'
            )


            if val['type'] == 'blob':
                self.buf += len(val['val_bytes'][0]).to_bytes(
                    self.constants['SIZE_SIZE'],
                    signed=False,
                    byteorder='little'
                )
            if dcount != 1:
                self.buf += dcount.to_bytes(
                    self.constants['COUNT_SIZE'],
                    signed=False,
                    byteorder='little'
                )

            for didx in range(dcount):
                if val['type'] == 'blob':
                    self.buf += val['val_bytes'][didx]
                elif val['type'] == 'float':
                    self.buf += struct.pack('f',val['value'][didx])
                elif val['type'] == 'double':
                    self.buf += struct.pack('d',val['value'][didx])
                else:
                    self.buf += val['value'][didx].to_bytes(
                        self.types[val['type']]['size'],
                        signed=self.types[val['type']]['signed'],
                        byteorder='little')

        self.__byteAssign('HD_OFFSET','HD_SIZE',self.constants['ID_VAL'])
        self.__byteAssign('VS_OFFSET','VS_SIZE',len(self.buf) - self.constants['V_OFFSET'])
        return self.buf

    def find(self,key):
         rv = self.vals.get(key,None)
         if rv is not None:
             return rv['value']
         return None

    # ----------------------------------------------------------
    ### end API
    # ----------------------------------------------------------

    def __bytesToInt(self,b,s = False):
        return int.from_bytes(b,signed=s,byteorder='little')

    def __getSizes(self):
        self.header = self.__bytesToInt(
            self.buf[self.constants['HD_OFFSET']:
                     self.constants['HD_OFFSET']+self.constants['HD_SIZE']
                    ],
        )
        self.vals_size = self.__bytesToInt(
            self.buf[self.constants['VS_OFFSET']:
                     self.constants['VS_OFFSET']+self.constants['VS_SIZE']
                    ],
        )

    def __scan(self):
        self.__getSizes();
        if (self.header & (0x7 << 3)) != (self.constants['ID_VAL'] & (0x7 << 3)):
            raise SDBException('incompatible bytestring')
        idx = self.constants['V_OFFSET'];
        rv = {};
        while idx < self.vals_size + self.constants['V_OFFSET']:
            key, = struct.unpack('H', self.buf[idx:idx+self.constants['KEY_SIZE']])
            idx += self.constants['KEY_SIZE']
            type_idx = self.__bytesToInt(self.buf[idx:idx+self.constants['TYPE_SIZE']])
            is_arry = type_idx & self.type_array_flag
            type_idx &= ~self.type_array_flag

            type_name = self.type_names[type_idx]
            idx += self.constants['TYPE_SIZE']
            if type_name == 'blob':
                dsize = self.__bytesToInt(self.buf[idx:idx+self.constants['SIZE_SIZE']])
                idx += self.constants['SIZE_SIZE']
            else:
                dsize = self.types[type_name]['size']

            dcount = 1
            if is_arry:
                dcount = self.__bytesToInt(self.buf[idx:idx+self.constants['COUNT_SIZE']])
                idx += self.constants['COUNT_SIZE']

            data_vals = []
            data_bytes = []
            datum_val = None
            for didx in range(dcount):
                datum_bytes = self.buf[idx:idx+dsize] 
                if type_name == 'blob':
                    datum_val = None
                elif type_name == 'float':
                    temp0 = struct.unpack('f',datum_bytes)
                    datum_val = temp0[0] 
                elif type_name == 'double':
                    temp1 = struct.unpack('d',datum_bytes)
                    datum_val = temp1[0] 
                else:
                    datum_val = self.__bytesToInt(datum_bytes,self.types[type_name]['signed'])
                data_vals.append(datum_val)
                data_bytes.append(datum_bytes)
                idx += dsize

            rv[key] = {
                'type': type_name or None,
                'value': data_vals,
                'val_bytes': data_bytes,
            }
        self.vals = rv;

    def __chunks(self,l,n):
        for i in range(0, len(l), n):
            yield l[i:i+n]

    def __dumpBytes(self,b):
        print("========")
        print("__dumpBytes len={}".format(len(b)))
        print('\n'.join((binascii.hexlify(x).decode('ascii') for x in self.__chunks(b, 32))))
        print("========")

        
    def __byteAssign(self,offset,size,value):
        self.buf[self.constants[offset] :
                 self.constants[offset] + self.constants[size]
                ] = value.to_bytes(self.constants[size],signed=False,byteorder='little')


def sdb_to_dict(b: bytes|bytearray) -> dict:
    return sdb(b).asDict()

def dict_to_sdb(d: dict) -> bytes:
    return sdb(d).toBytes()

import json
import binascii

class BytesEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, (bytearray,bytes)):
            return str(binascii.hexlify(obj),'ascii')
        return json.JSONEncoder.default(self, obj)

if __name__ == '__main__':

    import pprint

    for inname in ['t0','t1','t2','t3','t5','t6']:
        s = sdb('../c/' + inname + '.dat')
        s.saveToFile(inname + '_py1.dat')
        old_bytes = s.toBytes()
        t = sdb(old_bytes)
        t.saveToFile(inname + '_py2.dat')
        print(json.dumps(t.asDictDetailed(),sort_keys=True, indent=4,cls=BytesEncoder))
        print('old_bytes', binascii.hexlify(old_bytes))
        #print('simple_dict', json.dumps(sdb_to_dict(old_bytes), indent=2, sort_keys=True, cls=BytesEncoder))
        pprint.pprint(sdb_to_dict(old_bytes))
        s._sdb__dumpBytes(s.buf)


    d = {
        1: 11,
        2: -222,
        3: 3333,
        4: -44444,
        5: -555555,
        6: 6666666,
        7: -77777777,
        8: 888888888,
        9: -9999999999,
        10: [ 10, -100, 1000 ],
    }
    print(sdb_to_dict(dict_to_sdb(d)))




