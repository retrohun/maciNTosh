/*
 *
 * Open Hack'Ware BIOS ADB bus support, ported to OpenBIOS
 *
 *  Copyright (c) 2005 Jocelyn Mayer
 *  Copyright (c) 2005 Stefan Reinauer
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License V2
 *   as published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA, 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include "types.h"
#include "adb_bus.h"
#include "adb_kbd.h"
#include "pxi.h"

//#undef ADB_DPRINTF(fmt, args...)
//#define ADB_DPRINTF(fmt, args...) do { printf(fmt "\r", ##args); } while (0)

static adb_dev_t* g_adb_head;

/* Check and relocate all ADB devices as suggested in
 * ADB_manager Apple documentation
 */

int adb_bus_init(bool IsEmulator)
{
    char buf[64];
    uint8_t buffer[ADB_BUF_SIZE];
    uint8_t adb_addresses[16] =
        { 8, 9, 10, 11, 12, 13, 14, -1, -1, -1, -1, -1, -1, -1, 0, };
    adb_dev_t tmp_device, **cur;
    int address;
    int reloc = 0, next_free = 7;
    int keep;
    uint8_t subType;

    /* Reset the bus */
    // ADB_DPRINTF("\n");
    // On Mac99 do not reset the bus or relocate devices.
    // For Mac99 we must be a laptop with exactly one keyboard and one mouse connected.
    // Resetting and relocating on Mac99 systems have issues requiring PMU reset to resolve.
    //adb_reset();
    // ...except on emulator we have to, as the devices got moved...
    if (IsEmulator) adb_reset();
    cur = &g_adb_head;
    memset(&tmp_device, 0, sizeof(adb_dev_t));
    for (address = 1; address < 8 && adb_addresses[reloc] > 0;) {
        if (address == ADB_RES) {
            /* Reserved */
            address++;
            continue;
        }
        ADB_DPRINTF("Check device on ADB address %d\n", address);
        tmp_device.addr = address;
        switch (adb_reg_get(&tmp_device, 3, buffer)) {
        case 0:
            ADB_DPRINTF("No device on ADB address %d\n", address);
            /* Register this address as free */
            if (adb_addresses[next_free] != 0)
                adb_addresses[next_free++] = address;
            /* Check next ADB address */
            address++;
            break;
        case 2:
           /* One device answered :
            * make it available and relocate it to a free address
            */

            subType = buffer[1];
            if (buffer[0] == ADB_CHADDR) {
                /* device self test failed */
                ADB_DPRINTF("device on ADB address %d self-test failed "
                            "%02x %02x %02x\n", address,
                            buffer[0], buffer[1], buffer[2]);
                keep = 0;
            } else {
                //ADB_DPRINTF("device on ADB address %d self-test OK\n",
                //            address);
                keep = 1;
            }
#if 0
            ADB_DPRINTF("Relocate device on ADB address %d to %d (%d)\n",
                        address, adb_addresses[reloc], reloc);
            buffer[0] = ((buffer[0] & 0x40) & ~0x90) | adb_addresses[reloc];
            if (keep == 1)
                buffer[0] |= 0x20;
            buffer[1] = ADB_CHADDR_NOCOLL;
            if (adb_reg_set(&tmp_device, 3, buffer, 2) < 0) {
                ADB_DPRINTF("ADB device relocation failed\n");
                return -1;
            }
#endif
            if (keep == 1) {
                *cur = malloc(sizeof(adb_dev_t));
                if (*cur == NULL) {
                    return -1;
                }
                (*cur)->type = address;
                (*cur)->subType = subType;
                (*cur)->addr = address; // adb_addresses[reloc++];
                /* Flush buffers */
                adb_flush(*cur);
                switch ((*cur)->type) {
                case ADB_PROTECT:
                    ADB_DPRINTF("Found one protected device\n");
                    break;
                case ADB_KEYBD:
                    ADB_DPRINTF("Found one keyboard on address %d\n", address);
                    // Switch to extended protocol if possible
                    buffer[1] = 3;
                    if (adb_reg_set(&tmp_device, 3, buffer, 2) < 0) {
                        ADB_DPRINTF("Keyboard reg3 set failed\n");
                        break;
                    }
                    if (adb_reg_get(&tmp_device, 3, buffer) != 2) {
                        ADB_DPRINTF("Keyboard reg3 get failed\n");
                        break;
                    }
                    adb_kbd_new(buf, *cur);
                    break;
                case ADB_MOUSE:
                    ADB_DPRINTF("Found one mouse on address %d\n", address);
                    break;
                case ADB_ABS:
                    ADB_DPRINTF("Found one absolute positioning device\n");
                    break;
                case ADB_MODEM:
                    ADB_DPRINTF("Found one modem\n");
                    break;
                case ADB_RES:
                    ADB_DPRINTF("Found one ADB res device\n");
                    break;
                case ADB_MISC:
                    ADB_DPRINTF("Found one ADB misc device\n");
                    break;
                }
                cur = &((*cur)->next);
            }
            address++;
            break;
        case 1:
        case 3 ... 7:
            /* SHOULD NOT HAPPEN : register 3 is always two bytes long */
            ADB_DPRINTF("Invalid returned len for ADB register 3\n");
            return -1;
        case -1:
            /* ADB ERROR */
            ADB_DPRINTF("error gettting ADB register 3\n");
            return -1;
        }
    }

    return 0;
}

int adb_cmd (adb_dev_t *dev, uint8_t cmd, uint8_t reg,
             uint8_t *buf, int len)
{
    uint8_t adb_send[ADB_BUF_SIZE], adb_rcv[ADB_BUF_SIZE];

    //ADB_DPRINTF("cmd: %d reg: %d len: %d\n", cmd, reg, len);

    /* Sanity checks */
    if (cmd != ADB_LISTEN && len != 0) {
        /* No buffer transmitted but for LISTEN command */
        ADB_DPRINTF("in buffer for cmd %d\n", cmd);
        return -1;
    }
    if (cmd == ADB_LISTEN && ((len < 2 || len > 8) || buf == NULL)) {
        /* Need a buffer with a regular register size for LISTEN command */
        ADB_DPRINTF("no/invalid buffer for ADB_LISTEN (%d)\n", len);
        return -1;
    }
    if ((cmd == ADB_TALK || cmd == ADB_LISTEN) && reg > 3) {
        /* Need a valid register number for LISTEN and TALK commands */
        ADB_DPRINTF("invalid reg for TALK/LISTEN command (%d %d)\n", cmd, reg);
        return -1;
    }
    switch (cmd) {
    case ADB_SEND_RESET:
        adb_send[0] = ADB_SEND_RESET;
        break;
    case ADB_FLUSH:
        adb_send[0] = (dev->addr << 4) | ADB_FLUSH;
        break;
    case ADB_LISTEN:
        memcpy(adb_send + 1, buf, len);
        /* No break here */
    case ADB_TALK:
        adb_send[0] = (dev->addr << 4) | cmd | reg;
        break;
    }
    memset(adb_rcv, 0, ADB_BUF_SIZE);
    len = PxiAdbCommand(adb_send, len + 1, adb_rcv);
#ifdef DEBUG_ADB
    //printk("%x %x %x %x\n", adb_rcv[0], adb_rcv[1], adb_rcv[2], adb_rcv[3]);
#endif
    switch (len) {
    case 0:
        /* No data */
        break;
    case 2 ... 8:
        /* Register transmitted */
        if (buf != NULL)
            memcpy(buf, adb_rcv, len);
        break;
    default:
        /* Should never happen */
        //ADB_DPRINTF("Cmd %d returned %d bytes !\n", cmd, len);
        return -1;
    }
    //ADB_DPRINTF("retlen: %d\n", len);

    return len;
}
