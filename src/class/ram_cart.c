#include "ram_cart.h"

#define CART_RAM_BASE_ADDR ((volatile unsigned short *)0x22400000)
#define CART_RAM_1MB_LAST_WORD ((volatile unsigned short *)0x224FFFFE)
#define CART_RAM_4MB_LAST_WORD ((volatile unsigned short *)0x227FFFFE)

static bool ram_cart_probe_word(volatile unsigned short *addr, unsigned short pattern)
{
    unsigned short old_value;
    unsigned short read_back;

    if (addr == JO_NULL)
        return false;

    old_value = *addr;
    *addr = pattern;
    read_back = *addr;
    *addr = old_value;

    return read_back == pattern;
}

static bool ram_cart_probe_independent_words(volatile unsigned short *addr_a,
                                             volatile unsigned short *addr_b,
                                             unsigned short pattern_a,
                                             unsigned short pattern_b)
{
    unsigned short old_a;
    unsigned short old_b;
    unsigned short read_a;
    unsigned short read_b;

    if (addr_a == JO_NULL || addr_b == JO_NULL)
        return false;

    old_a = *addr_a;
    old_b = *addr_b;

    *addr_a = pattern_a;
    *addr_b = pattern_b;

    read_a = *addr_a;
    read_b = *addr_b;

    *addr_a = old_a;
    *addr_b = old_b;

    return read_a == pattern_a && read_b == pattern_b;
}

ram_cart_status_t ram_cart_detect(void)
{
    if (!ram_cart_probe_word(CART_RAM_BASE_ADDR, 0x55AA))
        return RamCartStatusNotDetected;

    if (ram_cart_probe_independent_words(CART_RAM_BASE_ADDR,
                                         CART_RAM_4MB_LAST_WORD,
                                         0x55AA,
                                         0xAA55))
    {
        return RamCartStatus4MB;
    }

    if (ram_cart_probe_word(CART_RAM_1MB_LAST_WORD, 0x5AA5))
        return RamCartStatus1MB;

    return RamCartStatusNotDetected;
}