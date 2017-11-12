/* Copyright 1995 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

#if !defined(OMIT_RCSID_STRINGS)
char ROMlib_rcsid_ibm_keycodes[] = "$Id: ibm_keycodes.c 63 2004-12-24 18:19:43Z ctm $";
#endif

#include "rsys/common.h"
#include "rsys/keyboard.h"

using namespace Executor;

#if defined(MSDOS) || defined(EVENT_SVGALIB) || defined(CYGWIN32) || (defined(SDL) && (defined(LINUX) || defined(MACOSX)))

unsigned char Executor::ibm_virt_to_mac_virt[] = {
    0xFF, /* unused 0x00 */
    0x35, /* ESC 0x01 */
    0x12, /* '1' 0x02 */
    0x13, /* '2' 0x03 */
    0x14, /* '3' 0x04 */
    0x15, /* '4' 0x05 */
    0x17, /* '5' 0x06 */
    0x16, /* '6' 0x07 */
    0x1A, /* '7' 0x08 */
    0x1C, /* '8' 0x09 */
    0x19, /* '9' 0x0a */
    0x1D, /* '0' 0x0b */
    0x1B, /* '-' 0x0c */
    0x18, /* '=' 0x0d */
    0x33, /* BACKSPACE 0x0e */
    0x30, /* TAB 0x0f */
    0x0C, /* 'Q' 0x10 */
    0x0D, /* 'W' 0x11 */
    0x0E, /* 'E' 0x12 */
    0x0F, /* 'R' 0x13 */
    0x11, /* 'T' 0x14 */
    0x10, /* 'Y' 0x15 */
    0x20, /* 'U' 0x16 */
    0x22, /* 'I' 0x17 */
    0x1F, /* 'O' 0x18 */
    0x23, /* 'P' 0x19 */
    0x21, /* '[' 0x1a */
    0x1E, /* ']' 0x1b */
    0x24, /* ENTER 0x1c */
    MKV_LEFTCNTL, /* 0x1d */
    0x00, /* 'A' 0x1e */
    0x01, /* 'S' 0x1f */
    0x02, /* 'D' 0x20 */
    0x03, /* 'F' 0x21 */
    0x05, /* 'G' 0x22 */
    0x04, /* 'H' 0x23 */
    0x26, /* 'J' 0x24 */
    0x28, /* 'K' 0x25 */
    0x25, /* 'L' 0x26 */
    0x29, /* ';' 0x27 */
    0x27, /* ''' 0x28 */
    0x32, /* '`' 0x29 */
    MKV_LEFTSHIFT, /* 0x2a */
    0x2A, /* '\' 0x2b */
    0x06, /* 'Z' 0x2c */
    0x07, /* 'X' 0x2d */
    0x08, /* 'C' 0x2e */
    0x09, /* 'V' 0x2f */
    0x0B, /* 'B' 0x30 */
    0x2D, /* 'N' 0x31 */
    0x2E, /* 'M' 0x32 */
    0x2B, /* ',' 0x33 */
    0x2F, /* '.' 0x34 */
    0x2C, /* '/' 0x35 */
    MKV_RIGHTSHIFT, /* 0x36 */
    0x4B, /* '*' 0x37 NUMERIC KEYPAD NOTE: '/' on Mac Keyboard*/
    MKV_CLOVER, /* unused 0x38 */
    0x31, /* ' ' 0x39 */
    MKV_CAPS, /* 0x3a */
    MKV_F1, /* unused 0x3b */
    MKV_F2, /* unused 0x3c */
    MKV_F3, /* unused 0x3d */
    MKV_F4, /* unused 0x3e */
    MKV_F5, /* unused 0x3f */
    MKV_F6, /* unused 0x40 */
    MKV_F7, /* unused 0x41	*/
    MKV_F8, /* unused 0x42 */
    MKV_F9, /* unused 0x43 */
    MKV_F10, /* unused 0x44 */
    0x47, /* NUM-LOCK 0x45 NUMERIC KEYPAD NOTE: CLEAR on Mac Keyboard */
    MKV_SCROLL_LOCK, /* 0x46 */
    0x59, /* '7' 0x47 NUMERIC KEYPAD */
    0x5B, /* '8' 0x48 NUMERIC KEYPAD */
    0x5C, /* '9' 0x49 NUMERIC KEYPAD */
    0x43, /* '-' 0x4a NUMERIC KEYPAD NOTE: '*' on Mac Keyboard */
    0x56, /* '4' 0x4b NUMERIC KEYPAD */
    0x57, /* '5' 0x4c NUMERIC KEYPAD */
    0x58, /* '6' 0x4d NUMERIC KEYPAD */
    0x4E, /* '+' 0x4e NUMERIC KEYPAD NOTE: '-' on Mac Keyboard */
    0x53, /* '1' 0x4f NUMERIC KEYPAD */
    0x54, /* '2' 0x50 NUMERIC KEYPAD */
    0x55, /* '3' 0x51 NUMERIC KEYPAD */
    0x52, /* '0' 0x52 NUMERIC KEYPAD */
    0x41, /* '.' 0x53 NUMERIC KEYPAD */
    0xFF, /* unused 0x54 */
    0xFF, /* unused 0x55 */
    0xFF, /* unused 0x56 */
    MKV_F11, /* unused 0x57 */
    MKV_F12, /* unused 0x58 */
    0xFF, /* unused 0x59 */
    0xFF, /* unused 0x5A */
    0xFF, /* unused 0x5B */
    0xFF, /* unused 0x5C */
    0xFF, /* unused 0x5D */
    0xFF, /* unused 0x5E */
    0xFF, /* unused 0x5F */
    MKV_NUMENTER, /* 0x60 */
    MKV_RIGHTCNTL, /* unused 0x61 */
    MKV_NUMEQUAL, /* 0x62 */
    MKV_PRINT_SCREEN, /* 0x63 */
    MKV_LEFTOPTION, /* 0x64 */
    0x7D, /* down arrow 0x65 NUMERIC KEYPAD */
    MKV_HOME, /* 0x66 */
    MKV_UPARROW, /* 0x67 */
    MKV_PAGEUP, /* 0x68 */
    MKV_LEFTARROW, /* unused 0x69 */
    MKV_RIGHTARROW, /* unused 0x6A */
    MKV_END, /* unused 0x6B */
    MKV_DOWNARROW, /* 0x6C */
    MKV_PAGEDOWN, /* 0x6D (SVGAlib) */
    MKV_HELP, /* 0x6E */
    MKV_DELFORWARD, /* 0x6F */
    0xFF, /* unused 0x70 */
    0xFF, /* unused 0x71 */
    0xFF, /* unused 0x72 */
    0xFF, /* unused 0x73 */
    0xFF, /* unused 0x74 */
    0xFF, /* unused 0x75 */
    MKV_PAGEDOWN, /* 0x76 (remapped in dosevents.c) */
    MKV_PAUSE, /* 0x77 */
};
#endif
