
/* shutdown manager define/routines */

namespace Executor
{
enum
{
    sdOnPowerOff = 1,
    sdOnRestart = 2,
    sdOnUnmount = 4,
    sdOnDrivers = 8,
    sdOnRestartOrPower = (sdOnPowerOff + sdOnRestart),
};

extern void C_ShutDwnPower(void);
PASCAL_SUBTRAP(ShutDwnPower, 0xA895, ShutDown);
extern void C_ShutDwnStart(void);
PASCAL_SUBTRAP(ShutDwnStart, 0xA895, ShutDown);
extern void C_ShutDwnInstall(ProcPtr shutdown_proc,
                                         int16_t flags);
PASCAL_SUBTRAP(ShutDwnInstall, 0xA895, ShutDown);
extern void C_ShutDwnRemove(ProcPtr shutdown_proc);
PASCAL_SUBTRAP(ShutDwnRemove, 0xA895, ShutDown);
}
