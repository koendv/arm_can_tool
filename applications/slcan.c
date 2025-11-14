/* slcan.c from canable-fw, ported to rt-thread */

//
// slcan: Parse incoming and generate outgoing slcan messages
//

#include <rtthread.h>
#include <rtdevice.h>
#include "usb_slcan.h"
#include "slcan.h"
#include "canbus.h"

#define DBG_TAG "SLCAN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define SLCAN_MTU        32
#define SLCAN_STD_ID_LEN 3
#define SLCAN_EXT_ID_LEN 8

// Parse an incoming CAN frame into an outgoing slcan message
rt_err_t slcan_parse_frame(struct rt_can_msg *msg)
{
    uint8_t buf[SLCAN_MTU];
    uint8_t msg_position = 0;

    for (uint8_t j = 0; j < SLCAN_MTU; j++)
    {
        buf[j] = '\0';
    }

    // Add character for frame type
    if (msg->rtr == RT_CAN_RTR)
    {
        buf[msg_position] = 'r';
    }
    else
    {
        buf[msg_position] = 't';
    }

    // Assume standard identifier
    uint8_t  id_len = SLCAN_STD_ID_LEN;
    uint32_t can_id = msg->id;

    // Check if extended
    if (msg->ide == RT_CAN_EXTID)
    {
        // Convert first char to upper case for extended frame
        buf[msg_position] -= 32;
        id_len             = SLCAN_EXT_ID_LEN;
        can_id             = msg->id;
    }
    msg_position++;

    // Add identifier to buffer
    for (uint8_t j = id_len; j > 0; j--)
    {
        // Add nybble to buffer
        buf[j] = (can_id & 0xF);
        can_id = can_id >> 4;
        msg_position++;
    }

    // Add DLC to buffer
    buf[msg_position++] = msg->len;

    // Add data bytes
    for (uint8_t j = 0; j < msg->len; j++)
    {
        buf[msg_position++] = (msg->data[j] >> 4);
        buf[msg_position++] = (msg->data[j] & 0x0F);
    }

    // Convert to ASCII (2nd character to end)
    for (uint8_t j = 1; j < msg_position; j++)
    {
        if (buf[j] < 0xA)
        {
            buf[j] += 0x30;
        }
        else
        {
            buf[j] += 0x37;
        }
    }

    // Add CR (slcan EOL)
    buf[msg_position++] = '\r';

    // send to usb
    slcan_send_reply(buf, msg_position);
    return RT_EOK;
}


// Parse an incoming slcan command coming from the USB CDC port
rt_err_t slcan_parse_str(uint8_t *buf, uint8_t len)
{
    struct rt_can_msg msg = {0};

    // Default to standard ID unless otherwise specified
    msg.ide = RT_CAN_STDID;
    msg.id  = 0;

    // Convert from ASCII (2nd character to end)
    for (uint8_t i = 1; i < len; i++)
    {
        // Lowercase letters
        if (buf[i] >= 'a')
            buf[i] = buf[i] - 'a' + 10;
        // Uppercase letters
        else if (buf[i] >= 'A')
            buf[i] = buf[i] - 'A' + 10;
        // Numbers
        else
            buf[i] = buf[i] - '0';
    }


    // Process command
    switch (buf[0])
    {
    case 'O':
        // Open channel command
        canbus_set_mode(RT_CAN_MODE_NORMAL);
        return RT_EOK;

    case 'C':
        // Close channel command
        canbus_set_mode(RT_CAN_MODE_LISTEN);
        return RT_EOK;

    case 'S':
        // Set bitrate command
        {
            if (len < 2) return -RT_EINVAL;

            // Map SLCAN bitrate codes to actual baudrates
            static const uint32_t bitrate_map[] = {
                10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000};

            uint8_t bitrate_code = buf[1];
            if (bitrate_code < sizeof(bitrate_map) / sizeof(bitrate_map[0]))
            {
                canbus_set_baudrate(bitrate_map[bitrate_code]);
                LOG_I("Bitrate set to %d", bitrate_map[bitrate_code]);
                return RT_EOK;
            }
            LOG_E("Invalid bitrate code: %d", bitrate_code);
            return -RT_EINVAL;
        }

    case 'm':
    case 'M':
        // Set mode command

        {
            if (len < 2) return -RT_EINVAL;
            if (buf[1] == 2)
            {
                canbus_set_mode(RT_CAN_MODE_LOOPBACK);
                LOG_I("Loopback mode enabled");
            }
            else if (buf[1] == 1)
            {
                canbus_set_mode(RT_CAN_MODE_LISTEN);
                LOG_I("Silent mode enabled");
            }
            else
            {
                canbus_set_mode(RT_CAN_MODE_NORMAL);
                LOG_I("Normal mode enabled");
            }
            return RT_EOK;
        }

    case 'a':
    case 'A':
        // Set autoretry command
        if (buf[1] == 1)
        {
            // Mode 1: autoretry enabled (default)
            canbus_set_autoretransmit(RT_TRUE);
        }
        else
        {
            // Mode 0: autoretry disabled
            canbus_set_autoretransmit(RT_FALSE);
        }
        return RT_EOK;

    case 'V': {
        // Report firmware version
        char *fw_id = "RT-Thread SLCAN v1.0\r";
        slcan_send_reply(fw_id, strlen(fw_id));
        return RT_EOK;
    }

#if 1
    // Hardware Filter Commands
    // F0 - begin filter list
    // F<mask3><value3> - standard filter
    // F<mask8><value8> - extended filter
    // F1 - end filter list
    case 'F': // Add hardware filter
    {
        if (len == 17)
        { // F<mask8><value8> Set extended filter
            uint32_t mask = 0, value = 0;
            for (uint8_t i = 0; i < 8; i++)
            {
                mask  = (mask << 4) | buf[1 + i];
                value = (value << 4) | buf[10 + i];
            }
            return canbus_set_ext_filter(value, mask);
        }
        else if (len == 7)
        { // F<mask3><value3> Set standard filter
            uint32_t mask  = (buf[1] << 8) | (buf[2] << 4) | buf[3];
            uint32_t value = (buf[5] << 8) | (buf[6] << 4) | buf[7];
            return canbus_set_std_filter(value, mask);
        }
        else if ((len == 2) && (buf[1] == 0))
        { // F0 Clear all hardware filters
            canbus_begin_filter();
            LOG_I("hardware filter clear");
            return RT_EOK;
        }
        else if ((len == 2) && (buf[1] == 1))
        { // F1 Set hardware filters
            canbus_end_filter();
            LOG_I("hardware filter set");
            return RT_EOK;
        }
        LOG_E("Invalid filter command format");
        return -RT_EINVAL;
    }

#endif

    case 'T':
        msg.ide = RT_CAN_EXTID;
        // Fall through
    case 't':
        // Transmit data frame command
        msg.rtr = RT_CAN_DTR; // XXX check
        break;

    case 'R':
        msg.ide = RT_CAN_EXTID;
    case 'r':
        // Transmit remote frame command
        msg.rtr = RT_CAN_RTR;
        break;

    default:
        // Error, unknown command
        return -1;
    }


    // Save CAN ID depending on ID type
    uint8_t msg_position = 1;
    if (msg.ide == RT_CAN_EXTID)
    {
        while (msg_position <= SLCAN_EXT_ID_LEN)
        {
            msg.id *= 16;
            msg.id += buf[msg_position++];
        }
    }
    else
    {
        while (msg_position <= SLCAN_STD_ID_LEN)
        {
            msg.id *= 16;
            msg.id += buf[msg_position++];
        }
    }


    // Attempt to parse DLC and check sanity
    msg.len = buf[msg_position++];
    if (msg.len > 8)
    {
        return -RT_ERROR;
    }

    // Copy frame data to buffer
    uint8_t frame_data[8] = {0};
    for (uint8_t j = 0; j < msg.len; j++)
    {
        frame_data[j]  = (buf[msg_position] << 4) + buf[msg_position + 1];
        msg_position  += 2;
    }

    // Transmit the message
    canbus_send_frame(&msg);

    return RT_EOK;
}

