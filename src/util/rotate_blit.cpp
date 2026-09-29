#include "./rotate_blit.h"

#include "extern/rotozoom/SDL_rotozoom.h"

#include <SDL/SDL.h>

#include <iostream>

namespace
{

inline uint32_t *pixel_row(SDL_Surface *surf, int y)
{
    return reinterpret_cast<uint32_t *>(
        reinterpret_cast<uint8_t *>(surf->pixels) + y * surf->pitch
    );
}

void rotate_180(SDL_Surface *src, SDL_Surface *dst)
{
    int w = src->w, h = src->h;
    for (int y = 0; y < h; ++y)
    {
        const uint32_t *src_row = pixel_row(src, y);
        uint32_t *dst_row = pixel_row(dst, h - 1 - y);
        for (int x = 0; x < w; ++x)
        {
            dst_row[w - 1 - x] = src_row[x];
        }
    }
}

void rotate_90(SDL_Surface *src, SDL_Surface *dst)
{
    int w = src->w, h = src->h;
    for (int y = 0; y < h; ++y)
    {
        const uint32_t *src_row = pixel_row(src, y);
        for (int x = 0; x < w; ++x)
        {
            pixel_row(dst, x)[h - 1 - y] = src_row[x];
        }
    }
}

void rotate_270(SDL_Surface *src, SDL_Surface *dst)
{
    int w = src->w, h = src->h;
    for (int y = 0; y < h; ++y)
    {
        const uint32_t *src_row = pixel_row(src, y);
        for (int x = 0; x < w; ++x)
        {
            pixel_row(dst, w - 1 - x)[y] = src_row[x];
        }
    }
}

} // namespace

void rotate_blit(SDL_Surface *src, SDL_Surface *dst, int degrees)
{
    if (degrees == 0)
    {
        return;
    }

    if (degrees == 180)
    {
        // The vendored rotozoom is used here instead of the hand-rolled
        // rotate_180 below because it is already relied on elsewhere
        // (cover thumbnails, in-book images). Its bounding-box math computes
        // sin(180 degrees) as a tiny non-zero value (never exactly zero in
        // floating point), which inflates a ceil() call in its output size
        // by one pixel per side - e.g. 642x482 instead of 640x480. That
        // padding lands entirely along the top and left edges of the
        // output, which the transform never writes to, so the real rotated
        // image is the bottom-right-aligned dst->w x dst->h region rather
        // than the top-left-aligned one.
        SDL_Surface *rotated = rotozoomSurface(src, 180.0, 1.0, 0);
        if (rotated)
        {
            int offset_x = rotated->w - dst->w;
            int offset_y = rotated->h - dst->h;
            if (offset_x >= 0 && offset_y >= 0)
            {
                SDL_Rect src_rect = {
                    static_cast<Sint16>(offset_x),
                    static_cast<Sint16>(offset_y),
                    static_cast<Uint16>(dst->w),
                    static_cast<Uint16>(dst->h)
                };
                SDL_BlitSurface(rotated, &src_rect, dst, NULL);
                SDL_FreeSurface(rotated);
                return;
            }

            std::cerr << "rotozoomSurface(180) returned smaller-than-expected size "
                << rotated->w << "x" << rotated->h << " (expected "
                << dst->w << "x" << dst->h << "), falling back to manual rotation"
                << std::endl;
            SDL_FreeSurface(rotated);
        }
        else
        {
            std::cerr << "rotozoomSurface(180) failed, falling back to manual rotation" << std::endl;
        }
        // fall through to the manual rotation below
    }

    SDL_LockSurface(src);
    SDL_LockSurface(dst);

    switch (degrees)
    {
        case 90:
            rotate_90(src, dst);
            break;
        case 180:
            rotate_180(src, dst);
            break;
        case 270:
            rotate_270(src, dst);
            break;
        default:
            break;
    }

    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
}
