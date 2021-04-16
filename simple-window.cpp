#include <stdio.h>
#include "window.h"

void *__disp_func__(void *data)
{
    struct window *w = (struct window *)data;
    printf("Start display thread.\n");
    pthread_mutex_lock(&w->mutex);
    while (w->running)
    {
        pthread_cond_wait(&w->cond, &w->mutex);
        if (!SDL_RenderCopy(w->renderer, w->texture, NULL, NULL))
            SDL_RenderPresent(w->renderer);
    }
    pthread_mutex_unlock(&w->mutex);
    printf("Stop display thread.\n");
    return NULL;
}

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

    pthread_mutex_init(&window->mutex, 0);
    window->cond = PTHREAD_COND_INITIALIZER;
    window->running = 1;
    pthread_create(&window->display_thread, NULL, __disp_func__, window);

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

int window_lock(struct window *w, void **addr)
{
    int pitch, ret;
    pthread_mutex_lock(&w->mutex);
    ret = SDL_LockTexture(w->texture, NULL, addr, &pitch);
    pthread_mutex_unlock(&w->mutex);
    return ret;
}

void window_unlock(struct window *w)
{
    pthread_mutex_lock(&w->mutex);
    SDL_UnlockTexture(w->texture);
    pthread_cond_signal(&w->cond);
    pthread_mutex_unlock(&w->mutex);
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
    pthread_mutex_lock(&window->mutex);
    window->running = 0;
    pthread_cond_signal(&window->cond);
    pthread_mutex_unlock(&window->mutex);
    pthread_join(window->display_thread, 0);
    printf("Display thread exit.\n");
    pthread_mutex_destroy(&window->mutex);
    SDL_DestroyTexture(window->texture);
    SDL_DestroyRenderer(window->renderer);
    SDL_DestroyWindow(window->window);
    SDL_Quit();
    free(window);
}
