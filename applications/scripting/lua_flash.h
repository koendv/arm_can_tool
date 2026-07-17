#ifndef LUA_FLASH_H
#define LUA_FLASH_H

#include <stdint.h>
#include <lua.h>

/*
 * lua_flash.h — Lua Flash Store (LFS) for AT32F405
 *
 * Flash sector size depends on the part number.
 * AT32F402/405 reference manual (Table 5-1/5-2):
 *   xB parts (128KB flash): 1KB physical sectors
 *   xC parts (256KB flash): 2KB physical sectors
 * Assume AT32F405xC: 256KB flash, 2KB sector size.
 *
 * 128KB of internal flash at 0x08020000, organised as 64 sectors of 2KB.
 * Firmware runs XIP from external QSPI flash, so the entire internal flash
 * above the 32KB bootloader is available.
 *
 * Each sector is self-contained:
 *
 *   offset  0: uint32_t magic    LFS_MAGIC if occupied, 0xFFFFFFFF if blank
 *   offset  4: uint32_t size     bytecode size in bytes
 *   offset  8: char     name[16] cosmetic label, 15 chars + NUL
 *   offset 24: uint8_t  bytecode[]
 *
 * Maximum bytecode size for sector N:
 *   (LFS_NUM_SECTORS - N) * LFS_SECTOR_SIZE - LFS_HEADER_SIZE
 *
 * A script may span multiple consecutive sectors.  list() shows
 * "continued" for sectors 2..n of a multi-sector script.
 *
 */

#define LFS_BASE        0x08020000UL
#define LFS_SECTOR_SIZE 0x0800UL     /* 2KB                        */
#define LFS_NUM_SECTORS 64           /* 128KB total                */
#define LFS_MAGIC       0x4C465300UL /* "LFS\0"                    */
#define LFS_BLANK       0xFFFFFFFFUL /* erased flash word          */
#define LFS_NAME_SIZE   16           /* 15 chars + NUL             */
#define LFS_HEADER_SIZE 24           /* sizeof(lfs_header_t)       */

typedef struct
{
    uint32_t magic;
    uint32_t size;
    char     name[LFS_NAME_SIZE];
} lfs_header_t;

static inline uint32_t lfs_sector_addr(int sector)
{
    return LFS_BASE + (uint32_t)sector * LFS_SECTOR_SIZE;
}

static inline const lfs_header_t *lfs_sector_header(int sector)
{
    return (const lfs_header_t *)lfs_sector_addr(sector);
}

/* maximum bytecode that fits starting at sector N */
static inline uint32_t lfs_max_size(int sector)
{
    return (uint32_t)(LFS_NUM_SECTORS - sector) * LFS_SECTOR_SIZE
           - LFS_HEADER_SIZE;
}

/* erase flash sector at address */
int lfs_hal_erase(uint32_t addr);

/* write a word-aligned buffer to flash at address */
int lfs_hal_write(uint32_t addr, const uint8_t *buf, size_t size);

/* load and execute bytecode from sector 0 */
int lfs_run_autoexec(lua_State *L);

/* luaopen entry point — call from script_engine.c after other luaopen_* */
int luaopen_flash_rotable(lua_State *L);

#endif /* LUA_FLASH_H */
