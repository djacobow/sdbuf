#!/usr/bin/env python3

import binascii
import json

class BytesEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, (bytearray,bytes)):
            return str(binascii.hexlify(obj),'ascii')
        return json.JSONEncoder.default(self, obj)

if __name__ == '__main__':
    import sdbuf;
    import pprint

    for inname in ['t0','t1','t2','t3','t5','t6']:
        s = sdbuf.sdb('../c/' + inname + '.dat')
        s.saveToFile(inname + '_py1.dat')
        old_bytes = s.toBytes()
        t = sdbuf.sdb(old_bytes)
        t.saveToFile(inname + '_py2.dat')
        print(json.dumps(t.asDictDetailed(),sort_keys=True, indent=4,cls=BytesEncoder))
        print('old_bytes', binascii.hexlify(old_bytes))
        pprint.pprint(sdbuf.sdb_to_dict(old_bytes))
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
    print(sdbuf.sdb_to_dict(sdbuf.dict_to_sdb(d)))




