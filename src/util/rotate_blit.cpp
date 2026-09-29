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
            if (rotated->w != dst->w || rotated->h != dst->h)
            {
                // rotozoomSurface's bounding-box math for a 180 degree turn
                // can be off by a pixel or two on some libm/platform
                // combinations (trig rounding), even though the rotation
                // itself is correct. Blit the overlapping region instead of
                // discarding the whole result over a near-miss - losing a
                // 1px border beats silently falling back to the unrotated-
                // looking manual path below.
                std::cerr << "rotozoomSurface(180) returned unexpected size "
                    << rotated->w << "x" << rotated->h << " (expected "
                    << dst->w << "x" << dst->h << "), blitting clipped"
                    << std::endl;
            }
            SDL_BlitSurface(rotated, NULL, dst, NULL);
            SDL_FreeSurface(rotated);
            return;
        }

        std::cerr << "rotozoomSurface(180) failed, falling back to manual rotation" << std::endl;
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
