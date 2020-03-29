#include "headless.h"

void HeadlessVideoDriver::setColors(int num_colors, const Executor::vdriver_color_t *color_array)
{
}
bool HeadlessVideoDriver::setMode(int width, int height, int bpp,
                                  bool grayscale_p)
{
    width_ = 512;
    height_ = 342;
    bpp_ = 1;
    rowBytes_ = ((width_ * bpp_ + 31) & ~31) / 8;
    framebuffer_ = new uint8_t[rowBytes_ * height_];
    return true;
}
