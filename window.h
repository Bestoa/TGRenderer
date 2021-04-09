#ifndef __WINDOW_TC__
#define __WINDOW_TC__

#include <SDL.h>
#include <SDL_image.h>

struct window {
    int width;
    int height;
    SDL_Window *window;
    SDL_Renderer *renderer;
    struct SDL_Texture *texture;
};

struct window *window_create(int width, int height);
int window_update(struct window *window, void *addr, size_t size);
int window_should_exit();
void window_destory(struct window *window);
#endif
