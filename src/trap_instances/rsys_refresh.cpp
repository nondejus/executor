#define INSTANTIATE_TRAPS_rsys_refresh
#include <rsys/refresh.h>

// Function for preventing the linker from considering the static constructors in this module unused
namespace Executor {
namespace ReferenceTraps {
    void rsys_refresh() {}
}
}