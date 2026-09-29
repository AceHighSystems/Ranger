

#ifndef INC_RANGER_CAN_H_
#define INC_RANGER_CAN_H_

#include <stdint.h>
#include <stdbool.h>


/*
 * Initialize the Ranger CAN interface and AceLight protocol instance.
 */
bool ranger_can_init(void);


/*
 * Transmit a CAN frame.
 *
 * This function is also used as the AceLight transmit callback.
 */
bool ranger_can_transmit(
    uint32_t identifier,
    const uint8_t *data,
    uint8_t length);


#endif /* INC_RANGER_CAN_H_ */
