#ifndef CANBUS_H
#define CANBUS_H

#include <stddef.h>

/* these are copied from linux headers and source:
 * linux-6.18.2/drivers/net/can.h 
 * linux-6.18.2/drivers/net/can/error.h 
 * linux-6.18.2/drivers/net/can/usb/gs_usb.c 
 */

#define __u8  uint8_t
#define __u16 uint16_t
#define __u32 uint32_t
#define __u64 uint64_t

#define u8     uint8_t
#define __le32 uint32_t

/**
 * @brief USB/CAN protocol classic CAN host frame
 */

struct gs_usb_host_frame
{
    /** Echo ID */
    uint32_t echo_id;
    /** CAN ID */
    uint32_t can_id;
    /** CAN DLC */
    uint8_t can_dlc;
    /** CAN channel */
    uint8_t channel;
    /** Host frame flags */
    uint8_t flags;
    /** Reserved */
    uint8_t reserved;
    /** Payload */
    uint8_t data[8];
} __packed __aligned(4);

typedef struct gs_usb_host_frame gs_usb_host_frame_t;

struct gs_usb_host_frame_timestamp
{
    /** Echo ID */
    uint32_t echo_id;
    /** CAN ID */
    uint32_t can_id;
    /** CAN DLC */
    uint8_t can_dlc;
    /** CAN channel */
    uint8_t channel;
    /** Host frame flags */
    uint8_t flags;
    /** Reserved */
    uint8_t reserved;
    /** Payload */
    uint8_t data[8];
    /** Timestamp in microseconds */
    uint32_t timestamp_us;
} __packed __aligned(4);

typedef struct gs_usb_host_frame_timestamp gs_usb_host_frame_timestamp_t;

/* from linux/include/uapi/linux/can.h */

/* special address description flags for the CAN_ID */
#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

/* from linux/include/uapi/linux/can/error.h */

#define CAN_ERR_DLC 8 /* dlc for error message frames */

/* error class (mask) in can_id */
#define CAN_ERR_TX_TIMEOUT 0x00000001U /* TX timeout (by netdevice driver) */
#define CAN_ERR_LOSTARB    0x00000002U /* lost arbitration    / data[0]    */
#define CAN_ERR_CRTL       0x00000004U /* controller problems / data[1]    */
#define CAN_ERR_PROT       0x00000008U /* protocol violations / data[2..3] */
#define CAN_ERR_TRX        0x00000010U /* transceiver status  / data[4]    */
#define CAN_ERR_ACK        0x00000020U /* received no ACK on transmission */
#define CAN_ERR_BUSOFF     0x00000040U /* bus off */
#define CAN_ERR_BUSERROR   0x00000080U /* bus error (may flood!) */
#define CAN_ERR_RESTARTED  0x00000100U /* controller restarted */
#define CAN_ERR_CNT        0x00000200U /* TX error counter / data[6] */
                                       /* RX error counter / data[7] */

/* arbitration lost in bit ... / data[0] */
#define CAN_ERR_LOSTARB_UNSPEC 0x00 /* unspecified */
                                    /* else bit number in bitstream */

/* error status of CAN-controller / data[1] */
#define CAN_ERR_CRTL_UNSPEC      0x00 /* unspecified */
#define CAN_ERR_CRTL_RX_OVERFLOW 0x01 /* RX buffer overflow */
#define CAN_ERR_CRTL_TX_OVERFLOW 0x02 /* TX buffer overflow */
#define CAN_ERR_CRTL_RX_WARNING  0x04 /* reached warning level for RX errors */
#define CAN_ERR_CRTL_TX_WARNING  0x08 /* reached warning level for TX errors */
#define CAN_ERR_CRTL_RX_PASSIVE  0x10 /* reached error passive status RX */
#define CAN_ERR_CRTL_TX_PASSIVE  0x20 /* reached error passive status TX */
                                      /* (at least one error counter exceeds */
                                      /* the protocol-defined level of 127)  */
#define CAN_ERR_CRTL_ACTIVE 0x40      /* recovered to error active state */

/* from linux/drivers/net/can/usb/gs_usb.c */

/* Device specific constants */
#define USB_GS_USB_1_VENDOR_ID  0x1d50
#define USB_GS_USB_1_PRODUCT_ID 0x606f

#define USB_CANDLELIGHT_VENDOR_ID  0x1209
#define USB_CANDLELIGHT_PRODUCT_ID 0x2323

#define USB_CES_CANEXT_FD_VENDOR_ID  0x1cd2
#define USB_CES_CANEXT_FD_PRODUCT_ID 0x606f

#define USB_ABE_CANDEBUGGER_FD_VENDOR_ID  0x16d0
#define USB_ABE_CANDEBUGGER_FD_PRODUCT_ID 0x10b8

#define USB_XYLANTA_SAINT3_VENDOR_ID  0x16d0
#define USB_XYLANTA_SAINT3_PRODUCT_ID 0x0f30

#define USB_CANNECTIVITY_VENDOR_ID  0x1209
#define USB_CANNECTIVITY_PRODUCT_ID 0xca01

/* Device specific constants */
enum gs_usb_breq
{
    GS_USB_BREQ_HOST_FORMAT = 0,
    GS_USB_BREQ_BITTIMING,
    GS_USB_BREQ_MODE,
    GS_USB_BREQ_BERR,
    GS_USB_BREQ_BT_CONST,
    GS_USB_BREQ_DEVICE_CONFIG,
    GS_USB_BREQ_TIMESTAMP,
    GS_USB_BREQ_IDENTIFY,
    GS_USB_BREQ_GET_USER_ID,
    GS_USB_BREQ_QUIRK_CANTACT_PRO_DATA_BITTIMING = GS_USB_BREQ_GET_USER_ID,
    GS_USB_BREQ_SET_USER_ID,
    GS_USB_BREQ_DATA_BITTIMING,
    GS_USB_BREQ_BT_CONST_EXT,
    GS_USB_BREQ_SET_TERMINATION,
    GS_USB_BREQ_GET_TERMINATION,
    GS_USB_BREQ_GET_STATE,
    GS_USB_BREQ_SET_FILTER,
    GS_USB_BREQ_GET_FILTER,
};

enum gs_can_mode
{
    /* reset a channel. turns it off */
    GS_CAN_MODE_RESET = 0,
    /* starts a channel */
    GS_CAN_MODE_START
};

enum gs_can_state
{
    GS_CAN_STATE_ERROR_ACTIVE = 0,
    GS_CAN_STATE_ERROR_WARNING,
    GS_CAN_STATE_ERROR_PASSIVE,
    GS_CAN_STATE_BUS_OFF,
    GS_CAN_STATE_STOPPED,
    GS_CAN_STATE_SLEEPING
};

enum gs_can_identify_mode
{
    GS_CAN_IDENTIFY_OFF = 0,
    GS_CAN_IDENTIFY_ON
};

enum gs_can_termination_state
{
    GS_CAN_TERMINATION_STATE_OFF = 0,
    GS_CAN_TERMINATION_STATE_ON
};

#define GS_USB_TERMINATION_DISABLED CAN_TERMINATION_DISABLED
#define GS_USB_TERMINATION_ENABLED  120

/* data types passed between host and device */

/* The firmware on the original USB2CAN by Geschwister Schneider
 * Technologie Entwicklungs- und Vertriebs UG exchanges all data
 * between the host and the device in host byte order. This is done
 * with the struct gs_host_config::byte_order member, which is sent
 * first to indicate the desired byte order.
 *
 * The widely used open source firmware candleLight doesn't support
 * this feature and exchanges the data in little endian byte order.
 */
struct gs_host_config
{
    __le32 byte_order;
} __packed;

struct gs_device_config
{
    u8     reserved1;
    u8     reserved2;
    u8     reserved3;
    u8     icount;
    __le32 sw_version;
    __le32 hw_version;
} __packed;

#define GS_CAN_MODE_NORMAL        0
#define GS_CAN_MODE_LISTEN_ONLY   BIT(0)
#define GS_CAN_MODE_LOOP_BACK     BIT(1)
#define GS_CAN_MODE_TRIPLE_SAMPLE BIT(2)
#define GS_CAN_MODE_ONE_SHOT      BIT(3)
#define GS_CAN_MODE_HW_TIMESTAMP  BIT(4)
/* GS_CAN_FEATURE_IDENTIFY BIT(5) */
/* GS_CAN_FEATURE_USER_ID BIT(6) */
#define GS_CAN_MODE_PAD_PKTS_TO_MAX_PKT_SIZE BIT(7)
#define GS_CAN_MODE_FD                       BIT(8)
/* GS_CAN_FEATURE_REQ_USB_QUIRK_LPC546XX BIT(9) */
/* GS_CAN_FEATURE_BT_CONST_EXT BIT(10) */
/* GS_CAN_FEATURE_TERMINATION BIT(11) */
#define GS_CAN_MODE_BERR_REPORTING BIT(12)
/* GS_CAN_FEATURE_GET_STATE BIT(13) */

struct gs_device_mode
{
    __le32 mode;
    __le32 flags;
} __packed;

struct gs_device_state
{
    __le32 state;
    __le32 rxerr;
    __le32 txerr;
} __packed;

struct gs_device_bittiming
{
    __le32 prop_seg;
    __le32 phase_seg1;
    __le32 phase_seg2;
    __le32 sjw;
    __le32 brp;
} __packed;

struct gs_identify_mode
{
    __le32 mode;
} __packed;

struct gs_device_termination_state
{
    __le32 state;
} __packed;

#define GS_CAN_FEATURE_LISTEN_ONLY              BIT(0)
#define GS_CAN_FEATURE_LOOP_BACK                BIT(1)
#define GS_CAN_FEATURE_TRIPLE_SAMPLE            BIT(2)
#define GS_CAN_FEATURE_ONE_SHOT                 BIT(3)
#define GS_CAN_FEATURE_HW_TIMESTAMP             BIT(4)
#define GS_CAN_FEATURE_IDENTIFY                 BIT(5)
#define GS_CAN_FEATURE_USER_ID                  BIT(6)
#define GS_CAN_FEATURE_PAD_PKTS_TO_MAX_PKT_SIZE BIT(7)
#define GS_CAN_FEATURE_FD                       BIT(8)
#define GS_CAN_FEATURE_REQ_USB_QUIRK_LPC546XX   BIT(9)
#define GS_CAN_FEATURE_BT_CONST_EXT             BIT(10)
#define GS_CAN_FEATURE_TERMINATION              BIT(11)
#define GS_CAN_FEATURE_BERR_REPORTING           BIT(12)
#define GS_CAN_FEATURE_GET_STATE                BIT(13)
#define GS_CAN_FEATURE_FILTER                   BIT(16)
#define GS_CAN_FEATURE_MASK                     GENMASK(13, 0)

/* internal quirks - keep in GS_CAN_FEATURE space for now */

/* CANtact Pro original firmware:
 * BREQ DATA_BITTIMING overlaps with GET_USER_ID
 */
#define GS_CAN_FEATURE_QUIRK_BREQ_CANTACT_PRO BIT(31)

struct gs_device_bt_const
{
    __le32 feature;
    __le32 fclk_can;
    __le32 tseg1_min;
    __le32 tseg1_max;
    __le32 tseg2_min;
    __le32 tseg2_max;
    __le32 sjw_max;
    __le32 brp_min;
    __le32 brp_max;
    __le32 brp_inc;
} __packed;

struct gs_device_bt_const_extended
{
    __le32 feature;
    __le32 fclk_can;
    __le32 tseg1_min;
    __le32 tseg1_max;
    __le32 tseg2_min;
    __le32 tseg2_max;
    __le32 sjw_max;
    __le32 brp_min;
    __le32 brp_max;
    __le32 brp_inc;

    __le32 dtseg1_min;
    __le32 dtseg1_max;
    __le32 dtseg2_min;
    __le32 dtseg2_max;
    __le32 dsjw_max;
    __le32 dbrp_min;
    __le32 dbrp_max;
    __le32 dbrp_inc;
} __packed;

#endif
