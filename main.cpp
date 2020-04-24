#include <cstdint>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <vector>
#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/fast_square_root.hpp>
#include "mtwister.h"

using namespace glm;

#define MAX_RAY_DEPTH 5
#define FRAME_W 320
#define FRAME_H 240

#define CHUNK_W 8
#define CHUNK_H 8
#define CHUNK_D 8

enum BlockTypes
{
    BT_AIR,
    BT_RED,
    BT_GLASS,
    BT_LIGHT,
    BT_COUNT
};

class BlockType
{
public:
    lowp_vec3 m_surfaceColor;
    lowp_vec3 m_emissionColor;
    float m_transparency;
    float m_reflection;
    bool m_solid;

    BlockType(
        const vec3 &surfaceColor,
        const vec3 &emissionColor,
        bool solid = true,
        float transparency = 0.0f,
        float reflection = 0.0f) : m_surfaceColor(surfaceColor),
                                   m_emissionColor(emissionColor),
                                   m_transparency(transparency),
                                   m_reflection(reflection),
                                   m_solid(solid)
    {
        //
    }
};

static MTRand mtRand = {0};
const BlockType types[BT_COUNT] = {
    BlockType(lowp_vec3(0), lowp_vec3(0), false),
    BlockType(lowp_vec3(1.0f, 1.0f, 0.0f), lowp_vec3(0), true, 0.7f, 0.1f),
    BlockType(lowp_vec3(0.0f, 1.0f, 1.0f), lowp_vec3(0), true, 0.7f, 0.1f),
    BlockType(lowp_vec3(1.0f, 0.0f, 1.0f), lowp_vec3(0), true, 0.7f, 0.1f),
};

class Scene
{
public:
    SDL_Surface *m_surface;
    unsigned int m_width;
    unsigned int m_height;
    float m_invWidth;
    float m_invHeight;
    float m_aspectRatio;
    float m_fov;
    lowp_vec3 m_position;
    lowp_vec3 m_rotation;
    lowp_vec3 m_skyColor;

    std::uint8_t m_blocks[CHUNK_W][CHUNK_H][CHUNK_D] = {0};
    std::vector<lowp_vec3> m_image;

    explicit Scene(SDL_Surface *surface) : m_surface(surface),
                                           m_width(static_cast<unsigned int>(surface->w)),
                                           m_height(static_cast<unsigned int>(surface->h)),
                                           m_invWidth(1.0f / m_width),
                                           m_invHeight(1.0f / m_height),
                                           m_aspectRatio(m_width / static_cast<float>(m_height)),
                                           m_fov(30.0f),
                                           m_position(0.0f, 3.0f, -10.0f),
                                           m_rotation(0.0f, 0.0f, 0.0f),
                                           m_skyColor(lowp_vec3(0.6f, 0.6f, 1.0f)),
                                           m_image(m_width * m_height)
    {
        //
        for (int i = 0; i < (CHUNK_W); i++)
        {
            for (int j = 0; j < (CHUNK_W); j++)
            {
                for (int k = 0; k < (CHUNK_W); k++)
                {
                    m_blocks[i][j][k] = rand() % BT_COUNT;
                }
            }
        }
    }

    bool intersect(const lowp_vec3 &bounds0, const lowp_vec3 &bounds1, const lowp_vec3 &rayOrig, const lowp_vec3 &rayDir, float &t0, float &t1)
    {
        vec3 dir_inv = 1.f / rayDir;

        float vt1 = (bounds0.x - rayOrig.x) * dir_inv.x;
        float vt2 = (bounds1.x - rayOrig.x) * dir_inv.x;

        float tmin = min(vt1, vt2);
        float tmax = max(vt1, vt2);

        for (int i = 1; i < 3; ++i)
        {
            vt1 = (bounds0[i] - rayOrig[i]) * dir_inv[i];
            vt2 = (bounds1[i] - rayOrig[i]) * dir_inv[i];

            tmin = max(tmin, min(vt1, vt2));
            tmax = min(tmax, max(vt1, vt2));
        }

        t0 = tmin;
        t1 = tmax;

        return tmax >= max(tmin, 0.f);
    }

    lowp_vec3 sample(const lowp_vec3 &orig, const lowp_vec3 &dir, const int depth)
    {
        const BlockType *type = nullptr;
        float tnear = INFINITY;
        lowp_vec3 surfaceColor;
        lowp_vec3 blockBounds0;
        lowp_vec3 blockBoundsCen;
        lowp_vec3 blockBounds1;

        for (unsigned bx = 0; bx < CHUNK_W; ++bx)
        {
            for (unsigned by = 0; by < CHUNK_H; ++by)
            {
                for (unsigned bz = 0; bz < CHUNK_D; ++bz)
                {
                    float t0 = -INFINITY;
                    float t1 = INFINITY;

                    std::uint8_t tile = m_blocks[bx][by][bz];
                    if (tile > BT_AIR && tile << BT_COUNT)
                    {
                        blockBounds0.x = bx;
                        blockBounds0.y = by;
                        blockBounds0.z = bz;
                        blockBoundsCen.x = bx + 0.5f;
                        blockBoundsCen.y = by + 0.5f;
                        blockBoundsCen.z = bz + 0.5f;
                        blockBounds1.x = bx + 1.0f;
                        blockBounds1.y = by + 1.0f;
                        blockBounds1.z = bz + 1.0f;

                        if (intersect(blockBounds0, blockBounds1, orig, dir, t0, t1))
                        {
                            if (t0 < 0)
                            {
                                t0 = t1;
                            }

                            if (t0 < tnear)
                            {
                                tnear = t0;
                                type = &types[tile];
                            }
                        }
                    }
                }
            }
        }

        // no intersection
        if (!type)
        {
            return m_skyColor;
        }

        lowp_vec3 phit = orig + dir * tnear;
        //lowp_vec3 nhit = fastNormalize(lowp_vec3(0.0f + rng(), -10.0f + rng(), 0.0f + rng()) + phit * -1.0f);
        if (type->m_transparency > 0.0f && depth < MAX_RAY_DEPTH)
        {
            //lowp_vec3 ref = lowp_vec3(dir.x + (genRand(&mtRand) - 0.5f) * 0.2f, dir.y, dir.z);
            lowp_vec3 back = sample(phit, dir, depth + 1);
            surfaceColor = (back * type->m_transparency) + (type->m_surfaceColor * (1 - type->m_transparency));
        } else {
            surfaceColor = type->m_surfaceColor;
        }

        return surfaceColor + type->m_emissionColor;
    }

    void move(lowp_vec3 direction)
    {
        m_position += direction;
    }

    void rotate(lowp_vec3 direction)
    {
        m_rotation += direction;
    }

    void render()
    {
        float angle = tanf(static_cast<float>(M_PI) * 0.5f * m_fov / 180.0f);
        uint32_t *pixels = reinterpret_cast<uint32_t *>(m_surface->pixels);
        
        for (unsigned x = 0; x < m_width; x++)
        {
            #pragma omp parallel for
            for (unsigned y = 0; y < m_height; y++)
            {
                lowp_vec3 rayDir;
                float xx = (2.0f * ((x + 0.5f) * m_invWidth) - 1.0f) * angle * m_aspectRatio;
                float yy = (1.0f - 2.0f * ((y + 0.5f) * m_invHeight)) * angle;

                rayDir.x = xx;
                rayDir.y = yy * cos(m_rotation.y) - sin(m_rotation.y);
                rayDir.z = yy * sin(m_rotation.y) + cos(m_rotation.y);

                rayDir = fastNormalize(rayDir);

                m_image[y * m_width + x] = 0.5f * (sample(m_position, rayDir, 0) + m_image[y * m_width + x]);
            }
        }

        for (unsigned x = 0; x < m_width; x++)
        {
            for (unsigned y = 0; y < m_height; y++)
            {
                // todo: do not assume pixel format
                unsigned int pos = y * m_width + x;
                pixels[pos] = (static_cast<uint32_t>(std::min(1.0f, m_image[pos].x) * 255.0f) << 16) | (static_cast<uint32_t>(std::min(1.0f, m_image[pos].y) * 255.0f) << 8) | (static_cast<uint32_t>(std::min(1.0f, m_image[pos].z) * 255.0f));
            }
        }
    }
};

#define AssertOrDie(COND, MESSAGE)                                              \
    if (COND)                                                                   \
    {                                                                           \
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", MESSAGE, NULL); \
        return 1;                                                               \
    }

int main(int argc, char *argv[])
{
    SDL_Window *win = nullptr;
    SDL_Surface *surface = nullptr;
    Scene *scene = nullptr;

    seedRand(&mtRand, clock());
    std::ios::sync_with_stdio(false);
    AssertOrDie(SDL_Init(SDL_INIT_VIDEO) < 0, "Cannot initialize SDL.");

    win = SDL_CreateWindow("blocktracer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, FRAME_W, FRAME_H, 0);
    AssertOrDie(!win, "Cannot create game window.");

    surface = SDL_GetWindowSurface(win);
    AssertOrDie(!surface, "No window surface returned?");

    scene = new Scene(surface);

    SDL_Event event;
    uint32_t frames = 0;
    uint32_t lastPrint = SDL_GetTicks();
    float rotSpeed = 0.1f;
    float speed = 0.5f;
    lowp_vec3 direction;
    lowp_vec3 rot_direction;
    bool running = true;

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                bool pressed = event.type == SDL_KEYDOWN;
                switch (event.key.keysym.scancode)
                {
                case SDL_SCANCODE_W:
                    direction.z = pressed ? speed : 0.0f;
                    break;
                case SDL_SCANCODE_S:
                    direction.z = pressed ? -speed : 0.0f;
                    break;
                case SDL_SCANCODE_D:
                    direction.x = pressed ? speed : 0.0f;
                    break;
                case SDL_SCANCODE_A:
                    direction.x = pressed ? -speed : 0.0f;
                    break;
                case SDL_SCANCODE_SPACE:
                    direction.y = pressed ? speed : 0.0f;
                    break;
                case SDL_SCANCODE_LSHIFT:
                    direction.y = pressed ? -speed : 0.0f;
                    break;
                case SDL_SCANCODE_LEFT:
                    rot_direction.x = pressed ? -rotSpeed : 0.0f;
                    break;
                case SDL_SCANCODE_RIGHT:
                    rot_direction.x = pressed ? rotSpeed : 0.0f;
                    break;
                case SDL_SCANCODE_UP:
                    rot_direction.y = pressed ? rotSpeed : 0.0f;
                    break;
                case SDL_SCANCODE_DOWN:
                    rot_direction.y = pressed ? -rotSpeed : 0.0f;
                    break;
                default:
                    break;
                }
                break;
            }
            default:
                break;
            }
        }

        if ((SDL_GetTicks() - lastPrint) > 1000)
        {
            lastPrint = SDL_GetTicks();
            printf("fps: %d\n", frames);
            frames = 0;
        }

        SDL_LockSurface(surface);
        scene->move(direction);
        scene->rotate(rot_direction);
        scene->render();
        ++frames;
        SDL_UnlockSurface(surface);
        SDL_UpdateWindowSurface(win);
    }

    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}