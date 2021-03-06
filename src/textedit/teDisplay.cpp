/* Copyright 1986, 1989, 1990, 1995 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

/* Forward declarations in TextEdit.h (DO NOT DELETE THIS LINE) */

#include <base/common.h>
#include <WindowMgr.h>
#include <ControlMgr.h>
#include <TextEdit.h>
#include <MemoryMgr.h>

#include <quickdraw/cquick.h>
#include <wind/wind.h>
#include <textedit/tesave.h>
#include <quickdraw/region.h>
#include <textedit/textedit.h>

using namespace Executor;

void Executor::C_TESetAlignment(INTEGER j, TEHandle teh)
{
    TE_SLAM(teh);
    (*teh)->just = j;
    TECalText(teh);
    TE_SLAM(teh);
}

void Executor::C_TEUpdate(const Rect *r, TEHandle te)
{
    TE_SLAM(te);
    if(!ROMlib_emptyvis)
    {
        TESAVE(te);

        TE_DO_TEXT(te, 0, TE_LENGTH(te), teDraw);
        if(TE_ACTIVE(te) && TE_CARET_STATE(te) != 255)
            ROMlib_togglelite(te);

        TERESTORE();
    }
    TE_SLAM(te);
}

void Executor::C_TETextBox(Ptr p, int32_t ln, const Rect *r, int16_t j)
{
    TEHandle teh;
    Rect viewrect;

    /* #### should we do anything at all here if !ROMlib_emptyvis? */

    viewrect = *r;

#if 0
  /*
   * The following InsetRect has been in here for a long time, but it
   * was causing trouble with CIM 2.1.4, so there's good reason to believe
   * that it never should have been here.  -- ctm
   */
  InsetRect(&viewrect, -3, -3);
#endif

    teh = TENew(r, &viewrect);
    TESetText(p, ln, teh);
    TESetAlignment(j, teh);
    if(!ROMlib_emptyvis)
        EraseRect(r);
    TEUpdate(r, teh);
    TEDispose(teh);
}

void Executor::C_TEScroll(int16_t dh, int16_t dv, TEHandle te)
{
    RgnHandle rh, save_vis;
    Rect r, vis_rgn_bbox;

    TESAVE(te);
    TE_SLAM(te);

    rh = NewRgn();

    r = TE_VIEW_RECT(te);

    ScrollRect(&r, dh, dv, rh);
    OffsetRect(&TE_DEST_RECT(te), dh, dv);

    save_vis = PORT_VIS_REGION(qdGlobals().thePort);
    SectRgn(rh, save_vis, rh);
    PORT_VIS_REGION(qdGlobals().thePort) = rh;

    vis_rgn_bbox = RGN_BBOX(PORT_VIS_REGION(qdGlobals().thePort));
    TEUpdate(&vis_rgn_bbox, te);

    PORT_VIS_REGION(qdGlobals().thePort) = save_vis;
    DisposeRgn(rh);

    TE_SLAM(te);
    TERESTORE();
}
