#if !defined(__WIND_H_)
#define __WIND_H_

/*
 * Copyright 1986, 1989, 1990 by Abacus Research and Development, Inc.
 * All rights reserved.
 *
 */

#include <WindowMgr.h>
#include <MemoryMgr.h>
#include <ResourceMgr.h>
#include <OSUtil.h>

#include <mman/mman.h>

#define MODULE_NAME wind_wind
#include <base/api-module.h>

namespace Executor
{
struct windrestype
{
    GUEST_STRUCT;
    GUEST<Rect> _wrect;
    GUEST<INTEGER> _wprocid;
    GUEST<char> _wvisible;
    GUEST<char> _padding1;
    GUEST<char> _wgoaway;
    GUEST<char> _padding2;
    GUEST<LONGINT> _wrefcon;
    GUEST<Byte> _wtitle;
};

extern LONGINT C_wdef0(INTEGER, WindowPtr, INTEGER, LONGINT);
extern LONGINT C_wdef16(INTEGER, WindowPtr, INTEGER, LONGINT);

extern WindowPeek ROMlib_firstvisible(WindowPtr w);

extern Boolean ROMlib_window_zoomed(WindowPeek wp);
extern void wind_color_init(void);

void ROMlib_rootless_update(RgnHandle extra = nullptr);
void ROMlib_rootless_openmenu(Rect r);
void ROMlib_rootless_closemenu();
bool ROMlib_rootless_drawdesk(RgnHandle desk);


typedef void (*draghookp)(void);

#define var(w) (GetWVariant((w)))

extern void CALLDRAGHOOK(void);
extern void WINDCALLDESKHOOK(void);
extern LONGINT ROMlib_windcall(WindowPtr wind, INTEGER mess, LONGINT param);

#define WINDCALL(w, m, x) ROMlib_windcall((w), (m), (x))

#define ZOOMBIT 8

#define USE_DESKCPAT_BIT 0x80
#define USE_DESKCPAT_VAR LM(SPMisc2)

extern const ColorSpec default_color_win_ctab[];
extern int ROMlib_emptyvis;

extern RGBColor *validate_colors_for_window(GrafPtr w);

/* the current window manager port being drawn to; since there are two
   windowmanager ports (bw and color), and since we couldn't hope to
   keep them in sync, we use one or the other.

   of course, our definition files use only the LM(WMgrCPort) */

//#define wmgr_port ((GrafPtr) (LM(WMgrCPort)))
#define wmgr_port (guest_cast<GrafPtr>(LM(WMgrCPort)))

/* WindowPeek accessors;
   note these functions cast thier argument to type (WindowPeek),
   so a cast is not necessary on the calling side */
#define WINDOW_PORT(wp) (&((WindowPeek)(wp))->port)
#define CWINDOW_PORT(wp) (&((WindowPeek)(wp))->port)

/* big endian byte order */
#define WINDOW_KIND(wp) (((WindowPeek)(wp))->windowKind)
#define WINDOW_VISIBLE(wp) (((WindowPeek)(wp))->visible)
/* does this mean the window has focus? */
#define WINDOW_HILITED(wp) (((WindowPeek)(wp))->hilited)
#define WINDOW_GO_AWAY_FLAG(wp) (((WindowPeek)(wp))->goAwayFlag)
#define WINDOW_SPARE_FLAG(wp) (((WindowPeek)(wp))->spareFlag)

#define WINDOW_STRUCT_REGION(wp) (((WindowPeek)(wp))->strucRgn)
#define WINDOW_CONT_REGION(wp) (((WindowPeek)(wp))->contRgn)
#define WINDOW_UPDATE_REGION(wp) (((WindowPeek)(wp))->updateRgn)
#define WINDOW_DEF_PROC(wp) (((WindowPeek)(wp))->windowDefProc)
#define WINDOW_DATA(wp) (((WindowPeek)(wp))->dataHandle)
#define WINDOW_TITLE(wp) (((WindowPeek)(wp))->titleHandle)
#define WINDOW_TITLE_WIDTH(wp) (((WindowPeek)(wp))->titleWidth)
#define WINDOW_CONTROL_LIST(wp) (((WindowPeek)(wp))->controlList)
#define WINDOW_NEXT_WINDOW(wp) (((WindowPeek)(wp))->nextWindow)
#define WINDOW_PIC(wp) (((WindowPeek)(wp))->windowPic)
#define WINDOW_REF_CON(wp) (((WindowPeek)(wp))->refCon)

static_assert(sizeof(windrestype) == 20);
}

#endif /* !_WIND_H_ */
