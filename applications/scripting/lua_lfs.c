#include <rtthread.h>
#define DBG_TAG "LUA"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "lua_lfs.h"
#include "lua_flash.h"
#include "serials.h"

/* -----------------------------------------------------------------------
** Flash loader protocol 
** --------------------------------------------------------------------- */

#define LFS_PROTO_VERSION    1
#define LFS_CMD_WRITE        0x01u /* lower 7 bits is command */
#define LFS_CMD_CONTINUATION 0x80u /* bit 7: set if more packets follow */
#define LFS_PACKET_DATA_SIZE LFS_SECTOR_SIZE

#define HDLC_FLAG       0x7Eu
#define HDLC_ESCAPE     0x7Du
#define HDLC_ESCAPE_XOR 0x20u

#define ACK 0x06u
#define NAK 0x15u

/* Flash loader packet — layout must match PC tool exactly.
** Total payload (before HDLC framing): 1 + 1 + 2 + 1024 + 4 = 1032 bytes. */

typedef struct __attribute__((packed))
{
    uint8_t  version;                    /* must be LFS_PROTO_VERSION     */
    uint8_t  command;                    /* LFS_CMD_WRITE | continuation  */
    uint16_t sector;                     /* destination sector 0..127     */
    uint8_t  data[LFS_PACKET_DATA_SIZE]; /* raw flash data */
    uint32_t crc;                        /* CRC-32 over preceding bytes   */
} lfs_packet_t;

#define LFS_PACKET_SIZE    sizeof(lfs_packet_t) /* 1032 */
#define LFS_PACKET_CRC_LEN (LFS_PACKET_SIZE - sizeof(uint32_t))

typedef enum
{
    STATE_SYNC,   /* waiting for opening 0x7E */
    STATE_DATA,   /* accumulating destuffed frame bytes */
    STATE_ESCAPE, /* next byte needs xor 0x20 */
} hdlc_state_t;

bool                 lfs_receiving = false;
static lfs_packet_t *lfs_pkt       = RT_NULL; /* allocated by lfs_lua_receive */
static hdlc_state_t  hdlc_state    = STATE_SYNC;
static size_t        hdlc_len      = 0;       /* bytes accumulated so far    */

void print_lfs_packet(const lfs_packet_t *pkt)
{
    rt_kprintf("version : %02X\n", pkt->version);
    rt_kprintf("command : %02X\n", pkt->command);
    rt_kprintf("sector  : %04X\n", pkt->sector);

    rt_kprintf("data    :\n");
    for (size_t i = 0; i < sizeof(pkt->data); i++)
    {
        rt_kprintf("%02X ", pkt->data[i]);
        if ((i + 1) % 16 == 0)
            rt_kprintf("\n");
    }

    rt_kprintf("\ncrc     : %08X\n", pkt->crc);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *buf, size_t len)
{
    crc = ~crc;
    while (len--)
    {
        crc ^= *buf++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
    }
    return ~crc;
}

bool lfs_receive_init()
{
    if (lfs_pkt == RT_NULL)
    {
        lfs_pkt = (lfs_packet_t *)rt_malloc(sizeof(lfs_packet_t));
    }

    if (lfs_pkt != RT_NULL)
    {
        rt_memset(lfs_pkt, 0, sizeof(lfs_packet_t));
    }

    lfs_receiving = lfs_pkt != NULL;
    hdlc_state    = STATE_SYNC;
    hdlc_len      = 0;

    return lfs_receiving;
}

static void hdlc_store_byte(uint8_t b)
{
    if (hdlc_len < LFS_PACKET_SIZE)
        ((uint8_t *)lfs_pkt)[hdlc_len] = b;
    hdlc_len++;
}

static void hdlc_ack(uint32_t s)
{
    uint8_t ack = ACK;
    cdc1_write((const char *)&ack, 1);
}

static void hdlc_nak(uint32_t s)
{
    uint8_t nak = NAK;
    cdc1_write((const char *)&nak, 1);
}

static void hdlc_process_frame()
{
    if (!lfs_pkt)
    {
        LOG_E("NULL packet");
        return;
    }

    if (hdlc_len != LFS_PACKET_SIZE)
    {
        LOG_E("sector %u bad length %u (expected %u)", lfs_pkt->sector, hdlc_len, LFS_PACKET_SIZE);
        return hdlc_nak(lfs_pkt->sector);
    }

    // print packet in hex for tracing
    //print_lfs_packet(lfs_pkt);

    if (lfs_pkt->version != LFS_PROTO_VERSION)
    {
        LOG_E("sector %u bad version %u (expected %u)", lfs_pkt->sector, lfs_pkt->version, LFS_PROTO_VERSION);
        return hdlc_nak(lfs_pkt->sector);
    }

    uint32_t command = lfs_pkt->command & ~LFS_CMD_CONTINUATION;
    if (command != LFS_CMD_WRITE)
    {
        LOG_E("sector %u bad command %u (expected %u)", lfs_pkt->sector, command, LFS_CMD_WRITE);
        return hdlc_nak(lfs_pkt->sector);
    }

    if (lfs_pkt->sector >= LFS_NUM_SECTORS)
    {
        LOG_E("sector %u bad sector (expected <= %u)", lfs_pkt->sector, LFS_NUM_SECTORS);
        return hdlc_nak(lfs_pkt->sector);
    }

    uint32_t computed = crc32_update(0, (const uint8_t *)lfs_pkt, LFS_PACKET_CRC_LEN);
    if (computed != lfs_pkt->crc)
    {
        LOG_E("sector %d bad crc", lfs_pkt->sector);
        return hdlc_nak(lfs_pkt->sector);
    }

    /* if continuation bit is zero, exit flash loader mode */
    lfs_receiving = (lfs_pkt->command & LFS_CMD_CONTINUATION) != 0;

    /* write frame to flash */

    if (lfs_hal_erase(lfs_sector_addr(lfs_pkt->sector)) != 0)
    {
        LOG_E("sector %d erase fail", lfs_pkt->sector);
        return hdlc_nak(lfs_pkt->sector);
    }

    if (lfs_hal_write(lfs_sector_addr(lfs_pkt->sector), lfs_pkt->data, LFS_PACKET_DATA_SIZE) != 0)
    {
        LOG_E("sector %d write fail", lfs_pkt->sector);
        return hdlc_nak(lfs_pkt->sector);
    }

    return hdlc_ack(lfs_pkt->sector);
}

void lfs_receive(uint8_t ch)
{
    switch (hdlc_state)
    {
    case STATE_SYNC:
        if (ch == HDLC_FLAG)
        {
            hdlc_len   = 0;
            hdlc_state = STATE_DATA;
        }
        break;

    case STATE_DATA:
        if (ch == HDLC_FLAG)
        {
            if (hdlc_len != 0)
                hdlc_process_frame();
            hdlc_len   = 0;
            hdlc_state = STATE_SYNC;
        }
        else if (ch == HDLC_ESCAPE)
        {
            hdlc_state = STATE_ESCAPE;
        }
        else
        {
            hdlc_store_byte(ch);
        }
        break;

    case STATE_ESCAPE:
        hdlc_store_byte(ch ^ HDLC_ESCAPE_XOR);
        hdlc_state = STATE_DATA;
        break;

    default:
        hdlc_state = STATE_SYNC;
        break;
    }
}

