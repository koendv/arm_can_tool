#ifndef __ROMDISK_H__
#define __ROMDISK_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ROM Disk Sparse Sector Structure */
struct romdisk_sparse_sector
{
    uint32_t lba;       /* Logical Block Address */
    uint8_t  data[512]; /* Sector data (always 512 bytes) */
};

/**
 * ROM Disk Auto-registration Macro
 * 
 * Usage in image header files:
 *   ROMDISK_IMAGE("no_card", no_card_sparse, 65536);
 * 
 * @param name          Device name (e.g., "no_card")
 * @param sparse_array  Array of sparse sectors
 * @param total_sectors Total number of sectors in the virtual disk
 */
#define ROMDISK_IMAGE(name, sparse_array, total_sectors)                                     \
    static int rt_romdisk_init_##sparse_array(void)                                          \
    {                                                                                        \
        uint32_t sparse_count = sizeof(sparse_array) / sizeof(struct romdisk_sparse_sector); \
        return rt_romdisk_create(#name, sparse_array, sparse_count, total_sectors);          \
    }                                                                                        \
    INIT_DEVICE_EXPORT(rt_romdisk_init_##sparse_array)

/**
 * @param name          Device name (must be unique)
 * @param sparse_table  Array of sparse sectors
 * @param sparse_count  Number of sparse sectors
 * @param total_sectors Total sectors in the virtual disk
 * @return RT_EOK on success, -RT_ERROR on failure
 */
int rt_romdisk_create(const char                         *name,
                      const struct romdisk_sparse_sector *sparse_table,
                      uint32_t                            sparse_count,
                      uint32_t                            total_sectors);

/* bash/python script to generate romdisk on linux.
::::::::::::::
mkimg
::::::::::::::
#!/bin/bash
dd if=/dev/zero of=fat32.img bs=1M count=32
mkfs.fat -F 32 \
  -n NO_CARD \
  -i 00000000 \
  fat32.img
echo "insert card" > NO_CARD.TXT
touch -t 198001010000 NO_CARD.TXT
mcopy -i fat32.img -D o -T NO_CARD.TXT ::
mdir -i fat32.img
python3 img_to_sparse_header.py fat32.img no_card_sparse.h
::::::::::::::
img_to_sparse_header.py
::::::::::::::
#!/usr/bin/env python3
import sys

SECTOR = 512

if len(sys.argv) != 3:
    print(f"usage: {sys.argv[0]} input.img output.h")
    sys.exit(1)

img_name = sys.argv[1]
out_name = sys.argv[2]

with open(img_name, "rb") as f:
    data = f.read()

sectors = []
for i in range(0, len(data), SECTOR):
    sector = data[i:i+SECTOR]
    if any(b != 0 for b in sector):
        sectors.append((i // SECTOR, sector))

with open(out_name, "w") as f:
    f.write("#pragma once\n")
    f.write("#include <stdint.h>\n\n")
    f.write("#define SPARSE_SECTOR_SIZE 512\n\n")
    f.write("struct sparse_sector {\n")
    f.write("    uint32_t lba;\n")
    f.write("    uint8_t  data[SPARSE_SECTOR_SIZE];\n")
    f.write("};\n\n")

    f.write("static const struct sparse_sector no_card_sparse[] = {\n")

    for lba, sector in sectors:
        f.write(f"    {{ {lba}, {{\n")
        for i in range(0, SECTOR, 16):
            row = ", ".join(f"0x{b:02x}" for b in sector[i:i+16])
            f.write(f"        {row},\n")
        f.write("    } },\n")

    f.write("};\n\n")
    f.write(f"static const uint32_t no_card_sparse_count = {len(sectors)};\n")
::::::::::::::
script_end
::::::::::::::

 */

#ifdef __cplusplus
}
#endif

#endif /* __ROMDISK_H__ */
