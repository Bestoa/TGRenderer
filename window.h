#ifndef __WINDOW_TC__
#define __WINDOW_TC__

#include <SDL.h>
#include <SDL_image.h>
#include <pthread.h>

struct window {
    int width;
    int height;
    SDL_Window *window;
    SDL_Renderer *renderer;
    struct SDL_Texture *texture;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t display_thread;
    int running;
};

struct window *window_create(int width, int height);
int window_lock(struct window *window, void **addr);
void window_unlock(struct window *window);
int window_should_exit();
void window_destory(struct window *window);
#endif
