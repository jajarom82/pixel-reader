#ifndef ROTATE_BLIT_H_
#define ROTATE_BLIT_H_

struct SDL_Surface;

// Rotate `src` clockwise by `degrees` (0, 90, 180 or 270) into `dst`.
// For 90/270, `dst` must have width == src->h and height == src->w.
// For 0/180, `dst` must have the same dimensions as `src`.
// Both surfaces must be 32bpp. Uses plain pixel copies (no interpolation),
// which is far cheaper than a general-purpose rotate for a full-screen
// blit performed on every render.
void rotate_blit(SDL_Surface *src, SDL_Surface *dst, int degrees);

#endif
