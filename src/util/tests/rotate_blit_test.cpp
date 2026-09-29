#include "../rotate_blit.h"

#include <gtest/gtest.h>

#include <SDL/SDL.h>

namespace
{

// One-time SDL setup shared by every test in this binary. Registering a
// global Environment (rather than defining our own main()) keeps this
// compatible with the shared gtest_main entry point the other test files
// already rely on.
class SDLEnvironment: public ::testing::Environment
{
public:
    void SetUp() override
    {
        SDL_Init(SDL_INIT_VIDEO);
    }

    void TearDown() override
    {
        SDL_Quit();
    }
};

::testing::Environment *const sdl_env = ::testing::AddGlobalTestEnvironment(new SDLEnvironment);

struct SurfaceGuard
{
    SDL_Surface *surface;
    explicit SurfaceGuard(SDL_Surface *s) : surface(s) {}
    ~SurfaceGuard() { if (surface) { SDL_FreeSurface(surface); } }
};

SDL_Surface *make_surface(int w, int h)
{
    return SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, 0, 0, 0, 0);
}

uint32_t get_pixel(SDL_Surface *surf, int x, int y)
{
    uint8_t *row = reinterpret_cast<uint8_t *>(surf->pixels) + y * surf->pitch;
    return reinterpret_cast<uint32_t *>(row)[x];
}

void set_pixel(SDL_Surface *surf, int x, int y, uint32_t value)
{
    uint8_t *row = reinterpret_cast<uint8_t *>(surf->pixels) + y * surf->pitch;
    reinterpret_cast<uint32_t *>(row)[x] = value;
}

// Fills a surface so pixel (x,y) holds a value encoding its own coordinates
// - any transposition/mirroring bug in a rotation then shows up as a
// mismatch against the by-hand expected source coordinate, rather than
// two rotations of a symmetric pattern accidentally looking identical.
void fill_with_coords(SDL_Surface *surf)
{
    for (int y = 0; y < surf->h; ++y)
    {
        for (int x = 0; x < surf->w; ++x)
        {
            set_pixel(surf, x, y, static_cast<uint32_t>(y * 1000 + x));
        }
    }
}

} // namespace

TEST(ROTATE_BLIT, rotate_180_is_point_reflection)
{
    const int w = 5, h = 3;
    SurfaceGuard src(make_surface(w, h));
    SurfaceGuard dst(make_surface(w, h));
    ASSERT_TRUE(src.surface);
    ASSERT_TRUE(dst.surface);

    fill_with_coords(src.surface);
    rotate_blit(src.surface, dst.surface, 180);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            SCOPED_TRACE(testing::Message() << "at (" << x << "," << y << ")");
            ASSERT_EQ(get_pixel(src.surface, x, y), get_pixel(dst.surface, w - 1 - x, h - 1 - y));
        }
    }
}

TEST(ROTATE_BLIT, rotate_90_is_clockwise)
{
    const int w = 5, h = 3;
    SurfaceGuard src(make_surface(w, h));
    SurfaceGuard dst(make_surface(h, w)); // swapped dims
    ASSERT_TRUE(src.surface);
    ASSERT_TRUE(dst.surface);

    fill_with_coords(src.surface);
    rotate_blit(src.surface, dst.surface, 90);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            SCOPED_TRACE(testing::Message() << "at (" << x << "," << y << ")");
            ASSERT_EQ(get_pixel(src.surface, x, y), get_pixel(dst.surface, h - 1 - y, x));
        }
    }
}

TEST(ROTATE_BLIT, rotate_270_is_counterclockwise)
{
    const int w = 5, h = 3;
    SurfaceGuard src(make_surface(w, h));
    SurfaceGuard dst(make_surface(h, w)); // swapped dims
    ASSERT_TRUE(src.surface);
    ASSERT_TRUE(dst.surface);

    fill_with_coords(src.surface);
    rotate_blit(src.surface, dst.surface, 270);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            SCOPED_TRACE(testing::Message() << "at (" << x << "," << y << ")");
            ASSERT_EQ(get_pixel(src.surface, x, y), get_pixel(dst.surface, y, w - 1 - x));
        }
    }
}

TEST(ROTATE_BLIT, rotate_0_is_a_noop)
{
    const int w = 4, h = 4;
    SurfaceGuard src(make_surface(w, h));
    SurfaceGuard dst(make_surface(w, h));
    ASSERT_TRUE(src.surface);
    ASSERT_TRUE(dst.surface);

    fill_with_coords(src.surface);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            set_pixel(dst.surface, x, y, 0xDEADBEEF);
        }
    }

    rotate_blit(src.surface, dst.surface, 0);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            ASSERT_EQ(0xDEADBEEFu, get_pixel(dst.surface, x, y));
        }
    }
}
