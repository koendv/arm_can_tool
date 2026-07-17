#ifndef CANDUMP_H
#define CANDUMP_H

#include "canbus_event.h"

/**
 * @brief Log a CAN frame to cdc0 in linux candump/canplayer format:
 *   (seconds.microseconds) can0 ID#DATA
 *
 * @param frame CAN frame to log.
 */
void candump_frame(const can_stored_frame_t *frame);

#endif /* CANDUMP_H */
