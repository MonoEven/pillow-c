"""BEHAV-OPEN-007 native cross-check: drive pillow_c_image_open_pcd from Python."""
import ctypes
import hashlib
import sys

sys.path.insert(0, 'oracle')
from probe_open_7 import pcd_blob, full_ycc, ycc_chunk

dll = ctypes.CDLL(r'build\x64\Release\pillow_c.dll')
dll.pillow_c_image_open_pcd.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_pcd.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_image_get_raw_bytes.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
dll.pillow_c_image_get_raw_bytes.restype = ctypes.c_int

lines = []

def check(name, blob, rawmode=b'RGB'):
    p = rf'oracle\_probe7_{name}.pcd'.encode()
    open(p.decode(), 'wb').write(blob)
    h = ctypes.c_void_p(0)
    st = dll.pillow_c_image_open_pcd(p, ctypes.byref(h))
    if st != 0:
        lines.append(f'{name}: status {st}')
        return
    req = ctypes.c_size_t(0)
    dll.pillow_c_image_get_raw_bytes(h, rawmode, None, 0, ctypes.byref(req))
    buf = (ctypes.c_ubyte * max(req.value, 1))()
    dll.pillow_c_image_get_raw_bytes(h, rawmode, buf, req.value, ctypes.byref(req))
    data = bytes(buf[:req.value])
    lines.append(f'{name}: OK md5={hashlib.md5(data).hexdigest()} len={len(data)} head={data[:24].hex()}')
    dll.pillow_c_image_free(h)

check('base', pcd_blob(full_ycc, 0))
check('orient1', pcd_blob(full_ycc, 1))
check('orient2', pcd_blob(full_ycc, 2))
check('orient3', pcd_blob(full_ycc, 3))
check('badmagic', pcd_blob(full_ycc, 0))
open(r'oracle\_probe7_bad.pcd', 'wb').write(b'\x00' * 2048 + b'NOPE' + b'\x00' * 2044)
h = ctypes.c_void_p(0)
st = dll.pillow_c_image_open_pcd(r'oracle\_probe7_bad.pcd'.encode(), ctypes.byref(h))
lines.append(f'badmagic2: status {st}')
check('trunc', pcd_blob(full_ycc[:256 * 2304 - 10000]))
check('half', pcd_blob(full_ycc[:128 * 2304]))

open(r'oracle/_probe7_dll.txt', 'w').write('\n'.join(lines) + '\n')
print('\n'.join(lines))
