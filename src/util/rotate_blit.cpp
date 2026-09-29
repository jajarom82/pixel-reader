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
        // Prefer the vendored, independently-maintained rotozoom for 180 -
        // it is already proven correct on real Miyoo Mini hardware (used
        // for cover thumbnails and in-book images), unlike the hand-rolled
        // rotate_180 below, which was reported to have no visible effect
        // on at least one real device despite passing automated tests on
        // desktop. Falls back to the manual version if this ever returns
        // something unexpected.
        SDL_Surface *rotated = rotozoomSurface(src, 180.0, 1.0, 0);
        if (rotated)
        {
            // rotozoomSurface's bounding-box math computes sin(180 degrees)
            // as a tiny non-zero value (a floating point artifact - it is
            // never exactly zero), which inflates a ceil() call in its size
            // calculation by one pixel per side, e.g. 642x482 instead of
            // 640x480. Confirmed by tracing its fixed-point transform math:
            // when the output is larger than the input, the padding this
            // introduces lands entirely along the top and left edges of the
            // output, and those padding rows/columns are never written by
            // the transform at all (left as whatever the freshly allocated
            // surface already contained). The actual rotated image is the
            // bottom-right-aligned dst->w x dst->h region, not the
            // top-left-aligned one - confirmed on-device by reading back
            // raw corner pixels: the bottom-right corner (the only one
            // outside the unwritten padding) held the mathematically
            // correct, correctly-rotated value, while the other three sat
            // inside the unwritten padding and read back stale/undefined
            // data left over from a previous frame's allocation, which is
            // what made the whole rotation look like it silently had no
            // effect.
            int offset_x = rotated->w - dst->w;
            int offset_y = rotated->h - dst->h;
            if (offset_x >= 0 && offset_y >= 0)
            {
                if (offset_x != 0 || offset_y != 0)
                {
                    std::cerr << "rotozoomSurface(180) returned padded size "
                        << rotated->w << "x" << rotated->h << " (expected "
                        << dst->w << "x" << dst->h << "), cropping to bottom-right region"
                        << std::endl;
                }
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
