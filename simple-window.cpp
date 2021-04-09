#include "window.h"

struct window * window_create(int width, int height)
{
    struct window *window = NULL;

    window = (struct window *)malloc(sizeof(struct window));
    if (window == NULL)
        return NULL;

    window->width = width;
    window->height = height;

    if (SDL_Init(SDL_INIT_VIDEO))
        goto free_window;

    window->window = SDL_CreateWindow("Tiny Raster",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
    if (window->window == NULL)
        goto deinit_sdl;

    window->renderer = SDL_CreateRenderer(window->window, -1, SDL_RENDERER_SOFTWARE);
    if (window->renderer == NULL)
        goto free_sdl_window;

    window->texture = SDL_CreateTexture(window->renderer, SDL_PIXELFORMAT_RGB24,
            SDL_TEXTUREACCESS_STREAMING, window->width, window->height);
    if (window->texture == NULL)
        goto free_renderer;

    return window;

free_renderer:
    SDL_DestroyRenderer(window->renderer);
free_sdl_window:
    SDL_DestroyWindow(window->window);
deinit_sdl:
    SDL_Quit();
free_window:
    free(window);
    return NULL;
}

int window_update(struct window *window, void *addr, size_t size)
{
    void *pixels;
    int pitch;
    if (SDL_LockTexture(window->texture, NULL, &pixels, &pitch))
        return 1;

    memcpy(pixels, addr, size);
    SDL_UnlockTexture(window->texture);
    if (SDL_RenderCopy(window->renderer, window->texture, NULL, NULL))
        return 1;

    SDL_RenderPresent(window->renderer);
    return 0;
}

int window_should_exit()
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        if (event.type == SDL_QUIT)
            return 1;
    }
    return 0;
}

void window_destory(struct window *window)
{
    SDL_DestroyTexture(window->texture);
    SDL_DestroyRenderer(window->renderer);
    SDL_DestroyWindow(window->window);
    SDL_Quit();
    free(window);
}
