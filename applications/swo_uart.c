#include <rtthread.h>
#include <rtconfig.h>
#define DBG_TAG "SWO"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "general.h"
#include "platform.h"
#include "swo.h"

#include "settings.h"
#include "serials.h"

/*
 uart7/swo is receive-only.
Uses at32 hal, bypasses rt-thread serial.
rt-thread serial:

- idle interrupt floods system
- two memcpy(): dma -> rt_ringbuffer -> user

hal:
- one static dma buffer
- no idle interrupt
- only dma half full/full interrupts
- runs swo_decode() on the raw dma buffers.

See AT32F402_405_Firmware_Library_V2.1.2/project/at_start_f405/examples/usart/transfer_by_dma_interrupt/

 */

#define SWO_DMA_BUFSIZE 2048

static uint8_t   swo_dma_buf[SWO_DMA_BUFSIZE] __attribute__((aligned(4)));
static rt_size_t swo_dma_tail  = 0;
rt_bool_t        serial2_ready = RT_FALSE;

void serial2_receive()
{
    rt_size_t head, tail;

    if (!serial2_ready)
        return;

    tail = swo_dma_tail;
    head = (SWO_DMA_BUFSIZE - dma_data_number_get(DMA1_CHANNEL7)) % SWO_DMA_BUFSIZE;

    if (head == tail)
        return;

    if (head > tail)
    {
        swo_itm_decode(&swo_dma_buf[tail], head - tail);
    }
    else
    {
        /* circular dma buffer wrapped */
        swo_itm_decode(&swo_dma_buf[tail], SWO_DMA_BUFSIZE - tail);
        if (head > 0)
            swo_itm_decode(&swo_dma_buf[0], head);
    }

    swo_dma_tail = head;
}

void serial2_enable(bool ena)
{
    if (serial2_ready)
        usart_receiver_mute_enable(UART7, !ena);
}

static rt_bool_t swo_gpio_ready = RT_FALSE;

static void swo_gpio_init(void)
{
    gpio_init_type gpio_init_struct;

    if (swo_gpio_ready)
        return;

    crm_periph_clock_enable(CRM_UART7_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOF_PERIPH_CLOCK, TRUE);

    /* gpio: rx-only, PF6, mux 9 */
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init_struct.gpio_pins           = GPIO_PINS_6;
    gpio_init(GPIOF, &gpio_init_struct);
    gpio_pin_mux_config(GPIOF, GPIO_PINS_SOURCE6, GPIO_MUX_9);

    swo_gpio_ready = RT_TRUE;
}

static rt_bool_t swo_dma_ready = RT_FALSE;

/* circular rx into swo_dma_buf */
static void swo_dma_init(void)
{
    dma_init_type dma_init_struct;

    if (swo_dma_ready)
        return;

    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);

    dma_reset(DMA1_CHANNEL7);
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.direction             = DMA_DIR_PERIPHERAL_TO_MEMORY;
    dma_init_struct.loop_mode_enable      = TRUE;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.memory_inc_enable     = TRUE;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.memory_data_width     = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.priority              = DMA_PRIORITY_MEDIUM;
    dma_init_struct.buffer_size           = SWO_DMA_BUFSIZE;
    dma_init_struct.memory_base_addr      = (uint32_t)swo_dma_buf;
    dma_init_struct.peripheral_base_addr  = (uint32_t)&UART7->dt;
    dma_init(DMA1_CHANNEL7, &dma_init_struct);

    dmamux_enable(DMA1, TRUE);
    dmamux_init(DMA1MUX_CHANNEL7, DMAMUX_DMAREQ_ID_UART7_RX);

    /* half-full and full interrupts, no idle-line interrupt */
    dma_interrupt_enable(DMA1_CHANNEL7, DMA_HDT_INT, TRUE);
    dma_interrupt_enable(DMA1_CHANNEL7, DMA_FDT_INT, TRUE);
    nvic_irq_enable(DMA1_Channel7_IRQn, 0, 0);

    dma_channel_enable(DMA1_CHANNEL7, TRUE);

    swo_dma_ready = RT_TRUE;
}

void serial2_set_speed(uint32_t speed)
{
    if (speed == 0)
        return;

    swo_gpio_init();

    /* usart: disable while reconfiguring. 8n1, no hardware flow control */
    usart_enable(UART7, FALSE);
    usart_init(UART7, speed, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_parity_selection_config(UART7, USART_PARITY_NONE);
    usart_hardware_flow_control_set(UART7, USART_HARDWARE_FLOW_NONE);
    usart_receiver_enable(UART7, TRUE);
    usart_dma_receiver_enable(UART7, TRUE);
    usart_enable(UART7, TRUE);

    swo_dma_init();

    swo_dma_tail  = (SWO_DMA_BUFSIZE - dma_data_number_get(DMA1_CHANNEL7)) % SWO_DMA_BUFSIZE;
    serial2_ready = RT_TRUE;

    LOG_I("uart7 speed %d", speed);
}

void DMA1_Channel7_IRQHandler(void)
{
    rt_interrupt_enter();

    DMA1->clr |= DMA1_FDT7_FLAG | DMA1_HDT7_FLAG;

    if (serial_event)
        rt_event_send(serial_event, EVENT_MASK_SERIAL2_RX);

    rt_interrupt_leave();
}

void swo_init(const swo_coding_e swo_mode, const uint32_t baudrate, const uint32_t itm_stream_bitmask)
{
    (void)swo_mode;

    serial2_set_speed(baudrate);
    swo_itm_decode_set_mask(itm_stream_bitmask);
}

void swo_deinit(const bool deallocate)
{
    (void)deallocate;

    swo_itm_decode_set_mask(0x0);
}

/* if configured in settings, start swo decoding at boot */
static int swd_boot_init(void)
{
    if (settings.swo_decode)
    {
        swo_itm_decode_set_mask(~0x0); /* all channels */
        LOG_I("init");
    }
    return RT_EOK;
}

INIT_APP_EXPORT(swd_boot_init);
