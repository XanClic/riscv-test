#include <assert.h>
#include <platform.h>
#include <platform-spike.h>
#include <platform-virt.h>


static PlatformType platform_type;
PlatformFuncs platform_funcs;


void init_platform(void)
{
    if (init_platform_virt()) {
        platform_type = PLATFORM_VIRT;
        return;
    }

    if (init_platform_spike()) {
        platform_type = PLATFORM_SPIKE;
        return;
    }

    assert(0);
}
