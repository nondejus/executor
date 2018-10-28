/* Copyright 1986-1995 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

/* Forward declarations in ControlMgr.h (DO NOT DELETE THIS LINE) */

#include "rsys/common.h"

#include "WindowMgr.h"
#include "ControlMgr.h"

#include "rsys/ctl.h"

using namespace Executor;

void Executor::C_SetControlValue(ControlHandle c, INTEGER v) /* IMI-326 */
{
    CtlCallGuard guard(c);
    if(v < Hx(c, contrlMin))
        HxX(c, contrlValue) = HxX(c, contrlMin);
    else if(v > Hx(c, contrlMax))
        HxX(c, contrlValue) = HxX(c, contrlMax);
    else
        HxX(c, contrlValue) = v;
    CTLCALL(c, drawCntl, ALLINDICATORS);

    EM_D0 = 0;
}

INTEGER Executor::C_GetControlValue(ControlHandle c) /* IMI-326 */
{
    return Hx(c, contrlValue);
}

void Executor::C_SetControlMinimum(ControlHandle c, INTEGER v) /* IMI-326 */
{
    CtlCallGuard guard(c);
    HxX(c, contrlMin) = v;
    if(Hx(c, contrlValue) < v)
        HxX(c, contrlValue) = v;
    CTLCALL(c, drawCntl, ALLINDICATORS);
}

INTEGER Executor::C_GetControlMinimum(ControlHandle c) /* IMI-327 */
{
    return Hx(c, contrlMin);
}

void Executor::C_SetControlMaximum(ControlHandle c, INTEGER v) /* IMI-327 */
{
    CtlCallGuard guard(c);
    /* #### TEST ON MAC MacBreadboard's behaviour suggests that
	   this code is needed. */
    if(v < Hx(c, contrlMin))
        v = Hx(c, contrlMin);

    HxX(c, contrlMax) = v;
    if(Hx(c, contrlValue) > v)
        HxX(c, contrlValue) = v;
    CTLCALL(c, drawCntl, ALLINDICATORS);
}

INTEGER Executor::C_GetControlMaximum(ControlHandle c) /* IMI-327 */
{
    return Hx(c, contrlMax);
}
