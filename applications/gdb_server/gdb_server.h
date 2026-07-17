#ifndef GDB_SERVER_H
#define GDB_SERVER_H

/* callback for usb stack */
void gdb_on_configured(uint8_t busid);

/* gdb polling interval */
rt_err_t gdb_set_polling_interval_ms(uint32_t time_ms);

/* halt running target, ISR-safe */
void gdb_target_halt_request(void);

#endif

