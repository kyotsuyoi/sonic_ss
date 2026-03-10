#ifndef RAM_CART_H
#define RAM_CART_H

#include <jo/jo.h>

typedef enum
{
    RamCartStatusNotDetected = 0,
    RamCartStatus1MB,
    RamCartStatus4MB
} ram_cart_status_t;

ram_cart_status_t ram_cart_detect(void);

#endif
