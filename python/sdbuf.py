#!/usr/bin/env python3

import struct
from sys import byteorder

class sdb:

    def __init__(self):
        self.buf = bytearray()
        self.constants = {
            'VER_MAJOR':  0x2,
            'VER_MINOR':  0x0,
            'SIZE_SIZE':  2,
            'COUNT_SIZE': 2,
            'TYPE_SIZE':  1,
            'KEY_SIZE':   2,
            'HD_SIZE'  :  1,
            'VS_SIZE'  :  4,
            'HD_OFFSET':  0, 
        }
        self.constants['VS_OFFSET'] = (
            self.constants['HD_OFFSET'] +
            self.constants['HD_SIZE']
        )
        self.constants['V_OFFSET'] = (
            self.constants['VS_OFFSET'] +
            self.constants['VS_SIZE']
        )
        self.constants['ID_VAL'] = (
            ((self.constants['VER_MAJOR'] & 0x7) << 3) |
            ((self.constants['VER_MINOR'] & 0x7) << 0) |
            (0x80 if byteorder == 'bib' else 0x0)
        )
        self.type_array_flag = 0x80;
        self.types = {
           's8':      { 'idx': 0,  'size': 1, 'signed': True  }, 
           's16':     { 'idx': 1,  'size': 2, 'signed': True  }, 
           's32':     { 'idx': 2,  'size': 4, 'signed': True  }, 
           's64':     { 'idx': 3,  'size': 8, 'signed': True  }, 
           'u8':      { 'idx': 4,  'size': 1, 'signed': False }, 
           'u16':     { 'idx': 5,  'size': 2, 'signed': False }, 
           'u32':     { 'idx': 6,  'size': 4, 'signed': False }, 
           'u64':     { 'idx': 7,  'size': 8, 'signed': False }, 
           'float':   { 'idx': 8,  'size': 4 }, 
           'double':  { 'idx': 9,  'size': 8 }, 
           'blob':    { 'idx': 10, 'size': 0 },
           '_inv':    { 'idx': 11, 'size': 0 },
        }
        self.type_names = [ x for x in self.types ]

    def initFromFile(self,fn):
        with open(fn,"rb") as ifh:
            self.buf = ifh.read()
        self._dumpBytes(self.buf)
        self._scan()

    def initFromBytes(self,b):
        self.buf = b;
        self._dumpBytes(self.buf)
        self._scan()

    # takes a scalar thing or a list of things of the same type
    def set(self,name,typename,value):
        if not isinstance(value,list):
            value = [value]

        self.vals[name] = {
            'type': self.types[typename]['idx'],
            'value': value,
        }

    # takes a bytearray or a list of bytearrays of equal length"
    def setBlob(self,name,vbytes):
        if not isinstance(vbytes,list):
            vbytes = [vbytes]
        self.vals[name] = {
            'type': self.types['blob']['idx'],
            'val_bytes': vbytes,
        }

    def getValues(self):
        return self.vals

    def debug(self):
        for name in self.vals:
            val = self.vals[name]
            if val['type'] == 'blob':
                print(" {:17} {:4} val {}".format(
                    name,val['type'],
                    self._bytesByFour(b''.join(self.vals[name]['val_bytes']))))
            else:
                print(" {:17} {:4} val {:20} {:30}".format(
                    name,
                    val['type'], 
                    ''.join(['[',','.join([str(x) for x in val['value']]),']']),
                    self._bytesByFour(list(reversed(b''.join(self.vals[name]['val_bytes'])))))
                )

    def saveToFile(self,fn):
        obytes = self.toBytes()
        with open(fn, 'wb') as ofh:
            ofh.write(obytes)

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
                    print('val',val['value'][didx])
                    self.buf += struct.pack('f',val['value'][didx])
                elif val['type'] == 'double':
                    self.buf += struct.pack('d',val['value'][didx])
                else:
                    self.buf += val['value'][didx].to_bytes(
                        self.types[val['type']]['size'],
                        signed=self.types[val['type']]['signed'],
                        byteorder='little')

        self._byteAssign('HD_OFFSET','HD_SIZE',self.constants['ID_VAL'])
        self._byteAssign('VS_OFFSET','VS_SIZE',len(self.buf) - self.constants['V_OFFSET'])
        return self.buf

    def find(self,key):
         rv = self.vals.get(key,None)
         if rv is not None:
             return rv['value']
         return None

    ### end API

    def _bytesToInt(self,b,s = False):
        return int.from_bytes(b,signed=s,byteorder='little')

    def _getSizes(self):
        self.header = self._bytesToInt(
            self.buf[self.constants['HD_OFFSET']:
                     self.constants['HD_OFFSET']+self.constants['HD_SIZE']
                    ],
        )
        print(self.buf[self.constants['VS_OFFSET']:self.constants['VS_OFFSET']+self.constants['VS_SIZE']])
        self.vals_size = self._bytesToInt(
            self.buf[self.constants['VS_OFFSET']:
                     self.constants['VS_OFFSET']+self.constants['VS_SIZE']
                    ],
        )

    def _readNullTermString(self,idx):
        string_done = False
        name_bytes = []
        while not string_done:
            inch = self.buf[idx]
            idx += 1
            if inch == 0:
                string_done = True
            else:
                name_bytes.append(inch)
        name_str = ''.join(map(lambda x: chr(x), name_bytes))
        return name_str

    def _scan(self):
        self._getSizes();
        idx = self.constants['V_OFFSET'];
        rv = {};
        while idx < self.vals_size + self.constants['V_OFFSET']:
            key, = struct.unpack('H', self.buf[idx:idx+self.constants['KEY_SIZE']])
            idx += self.constants['KEY_SIZE']
            type_idx = self._bytesToInt(self.buf[idx:idx+self.constants['TYPE_SIZE']])
            is_arry = type_idx & self.type_array_flag
            type_idx &= ~self.type_array_flag

            type_name = self.type_names[type_idx]
            idx += self.constants['TYPE_SIZE']
            if type_name == 'blob':
                dsize = self._bytesToInt(self.buf[idx:idx+self.constants['SIZE_SIZE']])
                idx += self.constants['SIZE_SIZE']
            else:
                dsize = self.types[type_name]['size']

            dcount = 1
            if is_arry:
                dcount = self._bytesToInt(self.buf[idx:idx+self.constants['COUNT_SIZE']])
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
                    datum_val = self._bytesToInt(datum_bytes,self.types[type_name]['signed'])
                data_vals.append(datum_val)
                data_bytes.append(datum_bytes)
                idx += dsize

            rv[key] = {
                'type': type_name or None,
                'value': data_vals,
                'val_bytes': data_bytes,
            }
        self.vals = rv;

    def _chunks(self,l,n):
        for i in range(0, len(l), n):
            yield l[i:i+n]

    def _bytesByFour(self,b):
        this_line_4b = self._chunks(b,4)
        this_line_hex = '_'.join(
            map(
                lambda outer: ''.join(
                    map(
                        lambda inner: format(inner,'x').zfill(2),
                        outer
                    ),
                ),
                this_line_4b 
            )
        )
        return this_line_hex


    def _dumpBytes(self,b):
        print("========")
        print("_dumpBytes len={}".format(len(b)))
        idx = 0
        while idx < len(b):
            line_end = idx + 16
            if line_end > len(b):
                line_end = len(b)
            this_line_b = b[idx:line_end]
            idx += 16
            this_line_hex = self._bytesByFour(this_line_b)
            
            print(this_line_hex)
        print("========")

        
    def _byteAssign(self,offset,size,value):
        self.buf[self.constants[offset] :
                 self.constants[offset] + self.constants[size]
                ] = value.to_bytes(self.constants[size],signed=False,byteorder='little')


import json
import base64

class BytesEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, bytearray):
            return str(base64.b64encode(obj),'ascii')
        if isinstance(obj, bytes):
            return str(base64.b64encode(obj),'ascii')
        return json.JSONEncoder.default(self, obj)

if __name__ == '__main__':

    for inname in ['t0','t1','t2','t3','t5']:
        s = sdb()
        s.initFromFile('../c/' + inname + '.dat')
        s.debug()
        s.saveToFile(inname + '_py1.dat')
        old_bytes = s.toBytes()
        t = sdb()
        t.initFromBytes(old_bytes)
        t.debug()
        t.saveToFile(inname + '_py2.dat')
        print(json.dumps(t.getValues(),sort_keys=True, indent=4,cls=BytesEncoder))
