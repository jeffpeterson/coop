
#include "esp_zigbee_core.h"
#include "light_driver.h"

/* Zigbee configuration */
#define LIGHT_ENDPOINT                                                         \
  10 /* esp light bulb device endpoint, used to process light controlling      \
        commands */

#define SHADE_ENDPOINT                                                         \
  11 /* esp shade device endpoint, used to process shade controlling           \
        commands */
