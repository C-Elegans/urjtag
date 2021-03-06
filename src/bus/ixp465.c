/*
 * $Id$
 *
 * Copyright (C) 2002 ETC s.r.o.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by Christian Pellegrin <chri@ascensit.com>, 2003.
 * Modified by Marcel Telka <marcel@telka.sk>, 2003.
 *
 */

#include "sysdep.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <urjtag/part.h>
#include <urjtag/bus.h>
#include <urjtag/chain.h>
#include <urjtag/bssignal.h>
#include <urjtag/jtag.h>

#include "buses.h"
#include "generic_bus.h"

typedef struct {
    urj_part_signal_t *ex_cs[8];
    urj_part_signal_t *ex_addr[24];
    urj_part_signal_t *ex_data[16];
    urj_part_signal_t *ex_wr;
    urj_part_signal_t *ex_rd;
} bus_params_t;

#define ex_cs_n ((bus_params_t *) bus->params)->ex_cs
#define ex_addr ((bus_params_t *) bus->params)->ex_addr
#define ex_data ((bus_params_t *) bus->params)->ex_data
#define ex_wr_n ((bus_params_t *) bus->params)->ex_wr
#define ex_rd_n ((bus_params_t *) bus->params)->ex_rd

/**
 * bus->driver->(*new_bus)
 *
 */
static urj_bus_t *
ixp465_bus_new (urj_chain_t *chain,
                const urj_bus_driver_t *driver,
                const urj_param_t *cmd_params[])
{
    urj_bus_t *bus;
    urj_part_t *part;
    char buff[15];
    int i;
    int failed = 0;

//  bus = calloc( 1, sizeof (urj_bus_t) );
//  if (!bus)
//      return NULL;

//  bus->driver = driver;
//  bus->params = calloc( 1, sizeof (bus_params_t) );
//  if (!bus->params) {
//      free( bus );
//      return NULL;
//  }

//  CHAIN = chain;
//  PART = part = chain->parts->parts[chain->active_part];

    bus = urj_bus_generic_new (chain, driver, sizeof (bus_params_t));
         if (bus == NULL)
                 return NULL;

    part = bus->part;

    for (i = 0; i < 8; i++) {
        sprintf (buff, "ex_cs_n_%d", i);
        failed |=  urj_bus_generic_attach_sig (part, &(ex_cs_n[i]), buff);
    }

    for (i = 0; i < 24; i++) {
        sprintf (buff, "ex_addr_%d", i);
        failed |=  urj_bus_generic_attach_sig (part, &(ex_addr[i]), buff);
    }

    for (i = 0; i < 16; i++) {
        sprintf (buff, "ex_data_%d", i);
        failed |=  urj_bus_generic_attach_sig (part, &(ex_data[i]), buff);
    }

    failed |=  urj_bus_generic_attach_sig (part, &(ex_wr_n), "ex_wr_n");

    failed |=  urj_bus_generic_attach_sig (part, &(ex_rd_n), "ex_rd_n");

    if (failed) {
         urj_bus_generic_free (bus);
        return NULL;
    }

    return bus;
}

/**
 * bus->driver->(*printinfo)
 *
 */
static void
ixp465_bus_printinfo (urj_log_level_t ll, urj_bus_t *bus)
{
 int i;

    for (i = 0; i < bus->chain->parts->len; i++)
        if (bus->part == bus->chain->parts->parts[i])
            break;
    urj_log (ll, _("Intel IXP465 compatible bus driver (JTAG part No. %d)\n"),
            i);

}

/**
 * bus->driver->(*area)
 *
 */
static int
ixp465_bus_area (urj_bus_t *bus, uint32_t adr, urj_bus_area_t *area)
{
    area->description = NULL;
    area->start = UINT32_C (0x00000000);
    area->length = UINT64_C (0x100000000);
    area->width = 16;

    return URJ_STATUS_OK;
}

static void
select_flash (urj_bus_t *bus)
{
    //urj_part_t *p = PART;
    urj_part_t *p = bus->part;
    urj_part_set_signal_low (p, ex_cs_n[0]);
    urj_part_set_signal_high (p, ex_cs_n[1]);
    urj_part_set_signal_high (p, ex_cs_n[2]);
    urj_part_set_signal_high (p, ex_cs_n[3]);
    urj_part_set_signal_high (p, ex_cs_n[4]);
    urj_part_set_signal_high (p, ex_cs_n[5]);
    urj_part_set_signal_high (p, ex_cs_n[6]);
    urj_part_set_signal_high (p, ex_cs_n[7]);
}

static void
unselect_flash (urj_bus_t *bus)
{
    urj_part_t *p = bus->part;

    urj_part_set_signal_high (p, ex_cs_n[0]);
    urj_part_set_signal_high (p, ex_cs_n[1]);
    urj_part_set_signal_high (p, ex_cs_n[2]);
    urj_part_set_signal_high (p, ex_cs_n[3]);
    urj_part_set_signal_high (p, ex_cs_n[4]);
    urj_part_set_signal_high (p, ex_cs_n[5]);
    urj_part_set_signal_high (p, ex_cs_n[6]);
    urj_part_set_signal_high (p, ex_cs_n[7]);
}

static void
setup_address (urj_bus_t *bus, uint32_t a)
{
    int i;
    urj_part_t *p = bus->part;

    for (i = 0; i < 24; i++)
        urj_part_set_signal (p, ex_addr[i], 1, (a >> i) & 1);
}

static void
set_data_in (urj_bus_t *bus)
{
    int i;
    urj_part_t *p = bus->part;

    for (i = 0; i < 16; i++)
        urj_part_set_signal_input (p, ex_data[i]);
}

static void
setup_data (urj_bus_t *bus, uint32_t d)
{
    int i;
    urj_part_t *p = bus->part;

    for (i = 0; i < 16; i++)
        urj_part_set_signal (p, ex_data[i], 1, (d >> i) & 1);
}

/**
 * bus->driver->(*read_start)
 *
 */
static int
ixp465_bus_read_start (urj_bus_t *bus, uint32_t adr)
{
    urj_part_t *p = bus->part;
    urj_chain_t *chain = bus->chain;

    select_flash( bus );
    urj_part_set_signal_low (p, ex_rd_n);
    urj_part_set_signal_high (p, ex_wr_n);

    setup_address (bus, adr);
    set_data_in (bus);

    urj_tap_chain_shift_data_registers (chain, 0);
    return URJ_STATUS_OK;
}

/**
 * bus->driver->(*read_next)
 *
 */
static uint32_t
ixp465_bus_read_next (urj_bus_t *bus, uint32_t adr)
{
    urj_part_t *p = bus->part;
    urj_chain_t *chain = bus->chain;
    int i;
    uint32_t d = 0;

    setup_address (bus, adr);
    urj_tap_chain_shift_data_registers (chain, 1);

    for (i = 0; i < 16; i++)
        d |= (uint32_t) (urj_part_get_signal (p, ex_data[i] ) << i);

    return d;
}

/**
 * bus->driver->(*read_end)
 *
 */
static uint32_t
ixp465_bus_read_end (urj_bus_t *bus)
{
    urj_part_t *p = bus->part;
    urj_chain_t *chain = bus->chain;
    int i;
    uint32_t d = 0;

    unselect_flash (bus);
    urj_part_set_signal_high (p, ex_rd_n);
    urj_part_set_signal_high (p, ex_wr_n);

    urj_tap_chain_shift_data_registers (chain, 1);

    for (i = 0; i < 16; i++)
        d |= (uint32_t) (urj_part_get_signal (p, ex_data[i] ) << i);

    return d;
}

/**
 * bus->driver->(*write)
 *
 */
static void
ixp465_bus_write (urj_bus_t *bus, uint32_t adr, uint32_t data)
{
    urj_part_t *p = bus->part;
    urj_chain_t *chain = bus->chain;

    select_flash (bus);
    urj_part_set_signal_high (p, ex_rd_n);

    setup_address (bus, adr);
    setup_data (bus, data);

    urj_tap_chain_shift_data_registers (chain, 0);

    urj_part_set_signal_low (p, ex_wr_n);
    urj_tap_chain_shift_data_registers (chain, 0);
    urj_part_set_signal_high (p, ex_wr_n);
    unselect_flash (bus);
    urj_tap_chain_shift_data_registers (chain, 0);
}

const urj_bus_driver_t urj_bus_ixp465_bus = {
    "ixp465",
    N_("Intel IXP465 compatible bus driver via BSR"),
    ixp465_bus_new,
    urj_bus_generic_free,
    ixp465_bus_printinfo,
    urj_bus_generic_prepare_extest,
    ixp465_bus_area,
    ixp465_bus_read_start,
    ixp465_bus_read_next,
    ixp465_bus_read_end,
    urj_bus_generic_read,
    urj_bus_generic_write_start,
    ixp465_bus_write,
    urj_bus_generic_no_init,
    urj_bus_generic_no_enable,
    urj_bus_generic_no_disable,
    URJ_BUS_TYPE_PARALLEL,
};
