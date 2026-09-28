#include "./rotate_blit.h"

#include <SDL/SDL.h>

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
