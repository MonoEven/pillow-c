"""Minimal CFB v3 (OLE2) container builder for the MIC oracle fixtures."""

import struct

MAGIC = b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1"
FREESECT = 0xFFFFFFFF
ENDOFCHAIN = 0xFFFFFFFE
FATSECT = 0xFFFFFFFD
DIFSECT = 0xFFFFFFFC

SECTOR = 512
MINI = 64
CUTOFF = 4096


def build_cfb(entries):
    """Build a minimal CFB v3 container.

    entries: list of (name, data) with full paths like "picture.ACI/Image".
    Streams >= CUTOFF live in regular FAT sectors; smaller streams live in
    the mini stream with a mini FAT stored in the mini stream's own first
    mini sectors (the conventional layout).
    """
    def insert(node, parts, data):
        head = parts[0]
        if head not in node:
            node[head] = [None, {}]
        if len(parts) == 1:
            node[head][0] = data
        else:
            insert(node[head][1], parts[1:], data)

    tree = {}
    for name, data in entries:
        insert(tree, name.split("/"), data)

    dir_entries = []
    stream_datas = []

    def walk(node, path_prefix):
        for cname in sorted(node):
            data, children = node[cname]
            etype = 2 if children == {} and data is not None else 1
            dir_entries.append((cname, etype, path_prefix + [cname]))
            if data is not None:
                stream_datas.append((path_prefix + [cname], data))
            if children:
                walk(children, path_prefix + [cname])

    walk(tree, [])
    full_entries = [("Root Entry", 5, [])] + dir_entries

    ndir_sectors = max(1, (len(full_entries) + 3) // 4)
    small = [(p, d) for p, d in stream_datas if len(d) < CUTOFF]
    big = [(p, d) for p, d in stream_datas if len(d) >= CUTOFF]

    dir_sector_ids = list(range(0, ndir_sectors))
    small = [(p, d) for p, d in stream_datas if len(d) < CUTOFF]
    big = [(p, d) for p, d in stream_datas if len(d) >= CUTOFF]

    # The mini FAT is a REGULAR stream (chained in the FAT, first sector in
    # the header at 60) whose entries cover the mini stream's 64-byte
    # sectors; the mini stream itself is the root entry's regular chain.
    mini_offset = {}
    nminifat = 0
    mfat_bytes = b""
    mini_stream_bytes = b""
    if small:
        total_stream_mini = sum((len(d) + MINI - 1) // MINI for _, d in small)
        nminifat = max(1, (total_stream_mini + 127) // 128)
        mfat = [ENDOFCHAIN] * (nminifat * 128)
        pos = 0
        for p, d in small:
            n = (len(d) + MINI - 1) // MINI
            start = pos
            mini_offset[tuple(p)] = start
            pos += n
            for i in range(n):
                mfat[start + i] = ENDOFCHAIN if i == n - 1 else start + i + 1
        mfat_bytes = b"".join(struct.pack("<I", v) for v in mfat)
        mini_stream_bytes = b"".join(
            d + b"\x00" * ((MINI - len(d) % MINI) % MINI) for _, d in small
        )

    data_sector_count = sum((len(d) + SECTOR - 1) // SECTOR for _, d in big)
    if small:
        data_sector_count += (len(mfat_bytes) + SECTOR - 1) // SECTOR
        data_sector_count += (len(mini_stream_bytes) + SECTOR - 1) // SECTOR
    nfat = max(1, (ndir_sectors + data_sector_count + 127) // 128)
    data_start = ndir_sectors
    fat_ids = list(range(data_start + data_sector_count, data_start + data_sector_count + nfat))

    cur = data_start
    stream_sectors = {}
    for p, d in big:
        n = (len(d) + SECTOR - 1) // SECTOR
        stream_sectors[tuple(p)] = list(range(cur, cur + n))
        cur += n
    mini_fat_sector = None
    if small:
        n = (len(mfat_bytes) + SECTOR - 1) // SECTOR
        mini_fat_sector = cur
        cur += n
    mini_stream_sector = None
    if small:
        n = (len(mini_stream_bytes) + SECTOR - 1) // SECTOR
        mini_stream_sector = cur
        cur += n

    fat = [FREESECT] * (nfat * 128)
    for f in fat_ids:
        fat[f] = FATSECT
    for i in range(ndir_sectors):
        fat[dir_sector_ids[i]] = ENDOFCHAIN if i == ndir_sectors - 1 else dir_sector_ids[i + 1]
    for ids in stream_sectors.values():
        for i, sid in enumerate(ids):
            fat[sid] = ENDOFCHAIN if i == len(ids) - 1 else ids[i + 1]
    if mini_fat_sector is not None:
        n = (len(mfat_bytes) + SECTOR - 1) // SECTOR
        for i in range(n):
            sid = mini_fat_sector + i
            fat[sid] = ENDOFCHAIN if i == n - 1 else sid + 1
    if mini_stream_sector is not None:
        n = (len(mini_stream_bytes) + SECTOR - 1) // SECTOR
        for i in range(n):
            sid = mini_stream_sector + i
            fat[sid] = ENDOFCHAIN if i == n - 1 else sid + 1

    header = bytearray([0xFF]) * SECTOR
    header[0:8] = MAGIC
    for i in range(9, 24):
        header[i] = 0
    struct.pack_into("<H", header, 24, 0x3E)
    struct.pack_into("<H", header, 26, 0x0003)
    struct.pack_into("<H", header, 28, 0xFFFE)
    struct.pack_into("<H", header, 30, 9)
    struct.pack_into("<H", header, 32, 6)
    struct.pack_into("<I", header, 44, nfat)
    struct.pack_into("<I", header, 48, dir_sector_ids[0])
    struct.pack_into("<I", header, 56, CUTOFF)
    if small:
        struct.pack_into("<I", header, 60, mini_fat_sector)
        struct.pack_into("<I", header, 64, nminifat)
    struct.pack_into("<I", header, 68, ENDOFCHAIN)
    struct.pack_into("<I", header, 72, 0)
    for i, fid in enumerate(fat_ids):
        struct.pack_into("<I", header, 76 + 4 * i, fid)

    child_map = {}
    for idx, (name, etype, path) in enumerate(full_entries):
        if len(path) >= 1:
            parent = tuple(path[:-1])
            child_map.setdefault(parent, []).append(idx)

    dir_bytes = bytearray(ndir_sectors * SECTOR)
    for idx, (name, etype, path) in enumerate(full_entries):
        e = bytearray(128)
        raw = (name + "\x00").encode("utf-16le")
        if len(raw) > 64:
            raw = raw[:64]
        e[0:len(raw)] = raw
        struct.pack_into("<H", e, 64, len(raw))
        e[66] = etype
        e[67] = 1  # black
        # red-black tree sentinels (NOSTREAM per the CFB spec)
        struct.pack_into("<I", e, 68, FREESECT)  # sid_left
        struct.pack_into("<I", e, 72, FREESECT)  # sid_right
        struct.pack_into("<I", e, 76, FREESECT)  # sid_child
        struct.pack_into("<I", e, 116, FREESECT)  # start
        children = child_map.get(tuple(path), [])
        if children:
            struct.pack_into("<I", e, 76, children[0])
        # left-leaning sibling chain: each child links its next sibling
        siblings = child_map.get(tuple(path[:-1]), [])
        for s in range(len(siblings) - 1):
            if siblings[s] == idx:
                struct.pack_into("<I", e, 68, siblings[s + 1])
        if etype == 5:
            if small and mini_stream_sector is not None:
                struct.pack_into("<I", e, 116, mini_stream_sector)
                struct.pack_into("<Q", e, 120, len(mini_stream_bytes))
        elif etype == 2:
            data = dict((tuple(p), d) for p, d in stream_datas).get(tuple(path))
            p = tuple(path)
            if p in mini_offset:
                struct.pack_into("<I", e, 116, mini_offset[p])
            elif p in stream_sectors:
                struct.pack_into("<I", e, 116, stream_sectors[p][0])
            if data is not None:
                struct.pack_into("<Q", e, 120, len(data))
        dir_bytes[idx * 128:(idx + 1) * 128] = bytes(e)

    fat_bytes = b"".join(struct.pack("<I", v) for v in fat)
    fat_bytes += b"\x00" * ((SECTOR - len(fat_bytes) % SECTOR) % SECTOR)

    payload = bytearray()
    payload += dir_bytes
    for p, d in big:
        payload += d + b"\x00" * ((SECTOR - len(d) % SECTOR) % SECTOR)
    if small:
        payload += mfat_bytes + b"\x00" * ((SECTOR - len(mfat_bytes) % SECTOR) % SECTOR)
        payload += mini_stream_bytes + b"\x00" * ((SECTOR - len(mini_stream_bytes) % SECTOR) % SECTOR)
    payload += fat_bytes

    blob = bytes(header) + bytes(payload)
    pad = (SECTOR - len(blob) % SECTOR) % SECTOR
    return blob + b"\x00" * pad
