"""BEHAV-OPEN-009 native cross-check: drive pillow_c_image_open_wmf from Python."""
import ctypes
import hashlib
import sys

sys.path.insert(0, 'oracle')
from probe_open_9 import placeable_wmf, metarecord, minimal_emf

dll = ctypes.CDLL(r'build\x64\Release\pillow_c.dll')
dll.pillow_c_image_open_wmf.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_wmf.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_image_get_raw_bytes.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
dll.pillow_c_image_get_raw_bytes.restype = ctypes.c_int

lines = []

def check(name, blob):
    p = rf'oracle\_dbg_wmf_{name}.wmf'.encode()
    open(p.decode(), 'wb').write(blob)
    h = ctypes.c_void_p(0)
    st = dll.pillow_c_image_open_wmf(p, ctypes.byref(h))
    if st != 0:
        lines.append(f'{name}: status {st}')
        return
    req = ctypes.c_size_t(0)
    dll.pillow_c_image_get_raw_bytes(h, b'RGB', None, 0, ctypes.byref(req))
    buf = (ctypes.c_ubyte * max(req.value, 1))()
    dll.pillow_c_image_get_raw_bytes(h, b'RGB', buf, req.value, ctypes.byref(req))
    data = bytes(buf[:req.value])
    lines.append(f'{name}: OK len={len(data)} md5={hashlib.md5(data).hexdigest()} nonwhite={sum(1 for b in data if b != 255)}')
    dll.pillow_c_image_free(h)

records = b""
records += metarecord(0x0103, [8])
records += metarecord(0x020B, [0, 0])
records += metarecord(0x020C, [100, 100])
records += metarecord(0x041B, [100, 100, 0, 0])
check('empty', placeable_wmf())
check('records', placeable_wmf(records=records))
check('inch0', placeable_wmf(inch=0))
check('sanity', placeable_wmf(sanity=b'\x00\x00\x00\x00'))
check('emf', minimal_emf())
print('\n'.join(lines))
