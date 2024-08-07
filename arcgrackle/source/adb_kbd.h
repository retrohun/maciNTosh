/*
 *
 * Open Hack'Ware BIOS ADB keyboard support, ported to OpenBIOS
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

typedef struct {
	UCHAR Modifiers;
	UCHAR Reserved;
	UCHAR KeyCode[6];
} USB_KBD_REPORT, * PUSB_KBD_REPORT;

void *adb_kbd_new (char *path, void *private);

UCHAR IOSKBD_ReadChar();
bool IOSKBD_CharAvailable();
void KBDOnEvent(PUSB_KBD_REPORT Report);