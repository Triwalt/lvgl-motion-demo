#include "lvgl.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include "motion_demo.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 480
#define FRAME_PERIOD_MS 8

static bool g_running = true;

static bool g_prev_up;
static bool g_prev_down;
static bool g_prev_left;
static bool g_prev_right;
static bool g_prev_enter;
static bool g_prev_add;
static bool g_prev_delete;
static bool g_prev_status;
static bool g_prev_next_pending;
static bool g_prev_approve;
static bool g_prev_reject;
static bool g_prev_escape;

static uint32_t g_last_up_ms;
static uint32_t g_last_down_ms;
static uint32_t g_last_left_ms;
static uint32_t g_last_right_ms;
static uint32_t g_last_enter_ms;
static uint32_t g_last_add_ms;
static uint32_t g_last_delete_ms;
static uint32_t g_last_status_ms;
static uint32_t g_last_next_pending_ms;
static uint32_t g_last_approve_ms;
static uint32_t g_last_reject_ms;
static uint32_t g_last_escape_ms;

static uint32_t key_cooldown_ms(uint32_t key)
{
    if(key == MOTION_DEMO_KEY_ADD || key == MOTION_DEMO_KEY_DELETE) {
        return 240;
    }
    return 80;
}

static bool dispatch_motion_key(uint32_t key, uint32_t * last_ms)
{
    uint32_t now = SDL_GetTicks();
    uint32_t cooldown = key_cooldown_ms(key);

    if(*last_ms != 0 && (uint32_t)(now - *last_ms) < cooldown) {
        return false;
    }

    *last_ms = now;
    motion_demo_handle_key(key);
    return true;
}

static void dispatch_key(SDL_Keycode key)
{
    if(key == SDLK_UP) {
        dispatch_motion_key(LV_KEY_UP, &g_last_up_ms);
    }
    else if(key == SDLK_DOWN) {
        dispatch_motion_key(LV_KEY_DOWN, &g_last_down_ms);
    }
    else if(key == SDLK_LEFT) {
        dispatch_motion_key(LV_KEY_LEFT, &g_last_left_ms);
    }
    else if(key == SDLK_RIGHT) {
        dispatch_motion_key(LV_KEY_RIGHT, &g_last_right_ms);
    }
    else if(key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        dispatch_motion_key(LV_KEY_ENTER, &g_last_enter_ms);
    }
    else if(key == SDLK_n) {
        dispatch_motion_key(MOTION_DEMO_KEY_ADD, &g_last_add_ms);
    }
    else if(key == SDLK_d) {
        dispatch_motion_key(MOTION_DEMO_KEY_DELETE, &g_last_delete_ms);
    }
    else if(key == SDLK_s) {
        dispatch_motion_key(MOTION_DEMO_KEY_STATUS, &g_last_status_ms);
    }
    else if(key == SDLK_TAB) {
        dispatch_motion_key(MOTION_DEMO_KEY_NEXT_PENDING, &g_last_next_pending_ms);
    }
    else if(key == SDLK_a) {
        dispatch_motion_key(MOTION_DEMO_KEY_APPROVE, &g_last_approve_ms);
    }
    else if(key == SDLK_r) {
        dispatch_motion_key(MOTION_DEMO_KEY_REJECT, &g_last_reject_ms);
    }
    else if(key == SDLK_ESCAPE) {
        dispatch_motion_key(MOTION_DEMO_KEY_ESCAPE, &g_last_escape_ms);
    }
}

static void handle_key_edge(bool pressed, bool * previous, uint32_t key, uint32_t * last_ms, bool suppress_press)
{
    if(pressed && !*previous && !suppress_press) {
        dispatch_motion_key(key, last_ms);
    }
    *previous = pressed;
}

static void poll_keyboard_state_edges(bool suppress_press)
{
    const Uint8 * state = SDL_GetKeyboardState(NULL);
    bool up = state[SDL_SCANCODE_UP] != 0;
    bool down = state[SDL_SCANCODE_DOWN] != 0;
    bool left = state[SDL_SCANCODE_LEFT] != 0;
    bool right = state[SDL_SCANCODE_RIGHT] != 0;
    bool enter = state[SDL_SCANCODE_RETURN] != 0 || state[SDL_SCANCODE_KP_ENTER] != 0;
    bool add = state[SDL_SCANCODE_N] != 0;
    bool delete_key = state[SDL_SCANCODE_D] != 0;
    bool status = state[SDL_SCANCODE_S] != 0;
    bool next_pending = state[SDL_SCANCODE_TAB] != 0;
    bool approve = state[SDL_SCANCODE_A] != 0;
    bool reject = state[SDL_SCANCODE_R] != 0;
    bool escape = state[SDL_SCANCODE_ESCAPE] != 0;

    handle_key_edge(up, &g_prev_up, LV_KEY_UP, &g_last_up_ms, suppress_press);
    handle_key_edge(down, &g_prev_down, LV_KEY_DOWN, &g_last_down_ms, suppress_press);
    handle_key_edge(left, &g_prev_left, LV_KEY_LEFT, &g_last_left_ms, suppress_press);
    handle_key_edge(right, &g_prev_right, LV_KEY_RIGHT, &g_last_right_ms, suppress_press);
    handle_key_edge(enter, &g_prev_enter, LV_KEY_ENTER, &g_last_enter_ms, suppress_press);
    handle_key_edge(add, &g_prev_add, MOTION_DEMO_KEY_ADD, &g_last_add_ms, suppress_press);
    handle_key_edge(delete_key, &g_prev_delete, MOTION_DEMO_KEY_DELETE, &g_last_delete_ms, suppress_press);
    handle_key_edge(status, &g_prev_status, MOTION_DEMO_KEY_STATUS, &g_last_status_ms, suppress_press);
    handle_key_edge(next_pending, &g_prev_next_pending, MOTION_DEMO_KEY_NEXT_PENDING, &g_last_next_pending_ms, suppress_press);
    handle_key_edge(approve, &g_prev_approve, MOTION_DEMO_KEY_APPROVE, &g_last_approve_ms, suppress_press);
    handle_key_edge(reject, &g_prev_reject, MOTION_DEMO_KEY_REJECT, &g_last_reject_ms, suppress_press);
    handle_key_edge(escape, &g_prev_escape, MOTION_DEMO_KEY_ESCAPE, &g_last_escape_ms, suppress_press);
}

#ifdef _WIN32
static bool foreground_window_is_this_process(void)
{
    HWND foreground = GetForegroundWindow();
    DWORD foreground_pid = 0;

    if(foreground == NULL) return false;
    GetWindowThreadProcessId(foreground, &foreground_pid);
    return foreground_pid == GetCurrentProcessId();
}

static bool win_key_down(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static void sync_polled_key_edges_to_current_state(void)
{
    g_prev_up = win_key_down(VK_UP);
    g_prev_down = win_key_down(VK_DOWN);
    g_prev_left = win_key_down(VK_LEFT);
    g_prev_right = win_key_down(VK_RIGHT);
    g_prev_enter = win_key_down(VK_RETURN);
    g_prev_add = win_key_down('N');
    g_prev_delete = win_key_down('D');
    g_prev_status = win_key_down('S');
    g_prev_next_pending = win_key_down(VK_TAB);
    g_prev_approve = win_key_down('A');
    g_prev_reject = win_key_down('R');
    g_prev_escape = win_key_down(VK_ESCAPE);
}

static void poll_win32_key_edges(bool suppress_press)
{
    if(!foreground_window_is_this_process()) {
        sync_polled_key_edges_to_current_state();
        return;
    }

    bool up = win_key_down(VK_UP);
    bool down = win_key_down(VK_DOWN);
    bool left = win_key_down(VK_LEFT);
    bool right = win_key_down(VK_RIGHT);
    bool enter = win_key_down(VK_RETURN);
    bool add = win_key_down('N');
    bool delete_key = win_key_down('D');
    bool status = win_key_down('S');
    bool next_pending = win_key_down(VK_TAB);
    bool approve = win_key_down('A');
    bool reject = win_key_down('R');
    bool escape = win_key_down(VK_ESCAPE);

    handle_key_edge(up, &g_prev_up, LV_KEY_UP, &g_last_up_ms, suppress_press);
    handle_key_edge(down, &g_prev_down, LV_KEY_DOWN, &g_last_down_ms, suppress_press);
    handle_key_edge(left, &g_prev_left, LV_KEY_LEFT, &g_last_left_ms, suppress_press);
    handle_key_edge(right, &g_prev_right, LV_KEY_RIGHT, &g_last_right_ms, suppress_press);
    handle_key_edge(enter, &g_prev_enter, LV_KEY_ENTER, &g_last_enter_ms, suppress_press);
    handle_key_edge(add, &g_prev_add, MOTION_DEMO_KEY_ADD, &g_last_add_ms, suppress_press);
    handle_key_edge(delete_key, &g_prev_delete, MOTION_DEMO_KEY_DELETE, &g_last_delete_ms, suppress_press);
    handle_key_edge(status, &g_prev_status, MOTION_DEMO_KEY_STATUS, &g_last_status_ms, suppress_press);
    handle_key_edge(next_pending, &g_prev_next_pending, MOTION_DEMO_KEY_NEXT_PENDING, &g_last_next_pending_ms, suppress_press);
    handle_key_edge(approve, &g_prev_approve, MOTION_DEMO_KEY_APPROVE, &g_last_approve_ms, suppress_press);
    handle_key_edge(reject, &g_prev_reject, MOTION_DEMO_KEY_REJECT, &g_last_reject_ms, suppress_press);
    handle_key_edge(escape, &g_prev_escape, MOTION_DEMO_KEY_ESCAPE, &g_last_escape_ms, suppress_press);
}
#endif

static void poll_input_events(void)
{
    SDL_Event event;
    bool handled_key_event = false;

    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT) {
            g_running = false;
        }
#ifndef _WIN32
        else if(event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            dispatch_key(event.key.keysym.sym);
            handled_key_event = true;
        }
#endif
    }

#ifndef _WIN32
    poll_keyboard_state_edges(handled_key_event);
#else
    poll_win32_key_edges(false);
#endif
}

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;

    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");

    lv_init();

    lv_display_t * display = lv_sdl_window_create(WINDOW_WIDTH, WINDOW_HEIGHT);
    if(display == NULL) {
        return 1;
    }

    motion_demo_create(lv_screen_active());

    if(getenv("LVGL_MOTION_SMOKE") != NULL) {
        return motion_demo_smoke_check() ? 0 : 2;
    }

    while(g_running) {
        motion_demo_tick(SDL_GetTicks());

        uint32_t wait_ms = lv_timer_handler();
        poll_input_events();
        motion_demo_tick(SDL_GetTicks());

        if(wait_ms == LV_NO_TIMER_READY) {
            wait_ms = 5;
        }
        if(wait_ms > FRAME_PERIOD_MS) {
            wait_ms = FRAME_PERIOD_MS;
        }
        SDL_Delay(wait_ms);
    }

    return 0;
}
