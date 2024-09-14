#pragma once

#include "common.h"

#include "opengl.h"
#include <GLFW/glfw3.h>

enum Event_Type : u32
{
    EVENT_UNINITIALIZED = 0,
    EVENT_KEYBOARD      = 1,
    EVENT_TEXT_INPUT    = 2,
    EVENT_MOUSE_V_WHEEL = 3,
    EVENT_MOUSE_H_WHEEL = 4,
};

enum Key_Current_State : u32
{
    KSTATE_NONE  = 0x0,
    KSTATE_DOWN  = 0x1,
    KSTATE_START = 0x4,
    KSTATE_END   = 0x8,
};

// @Important: Not Unicode's compliant
enum Key_Code : u32
{
    CODE_UNKNOWN = 0,

    CODE_BACKSPACE ,// = 8,
    CODE_TAB       ,// = 9,
    CODE_ENTER     ,// = 13,
    CODE_ESCAPE    ,// = 27,
    CODE_SPACEBAR  ,// = 32,

    // The letters A-Z live in here as well and may be returned by keyboard events.
    // The ASCII characters are currently not mapped to value 

    CODE_0,
    CODE_1,
    CODE_2,
    CODE_3,
    CODE_4,
    CODE_5,
    CODE_6,
    CODE_7,
    CODE_8,
    CODE_9,

    CODE_A,
    CODE_B,
    CODE_C,
    CODE_D,
    CODE_E,
    CODE_F,
    CODE_G,
    CODE_H,
    CODE_I,
    CODE_J,
    CODE_K,
    CODE_L,
    CODE_M,
    CODE_N,
    CODE_O,
    CODE_P,
    CODE_Q,
    CODE_R,
    CODE_S,
    CODE_T,
    CODE_U,
    CODE_V,
    CODE_W,
    CODE_X,
    CODE_Y,
    CODE_Z,

    CODE_DELETE       ,// = 127,
                       // 
    CODE_ARROW_UP     ,// = 128,
    CODE_ARROW_DOWN   ,// = 129,
    CODE_ARROW_LEFT   ,// = 130,
    CODE_ARROW_RIGHT  ,// = 131,
                       // 
    CODE_PAGE_UP      ,// = 132,
    CODE_PAGE_DOWN    ,// = 133,
                       // 
    CODE_HOME         ,// = 134,
    CODE_END          ,// = 135,
                       // 
    CODE_INSERT       ,// = 136,
                       // 
    CODE_PAUSE        ,// = 137,
    CODE_SCROLL_LOCK  ,// = 138,

    CODE_ALT,
    CODE_CTRL,
    CODE_SHIFT,

    CODE_F1,
    CODE_F2,
    CODE_F3,
    CODE_F4,
    CODE_F5,
    CODE_F6,
    CODE_F7,
    CODE_F8,
    CODE_F9,
    CODE_F10,
    CODE_F11,
    CODE_F12,

    CODE_LEFT_BRACKET,
    CODE_RIGHT_BRACKET,

    CODE_SEMICOLON,
    CODE_QUOTE,

    CODE_MOUSE_RIGHT,
    CODE_MOUSE_LEFT,

    CODE_TOTAL_COUNT
};

struct Event
{
    bool shift_pressed = false;
    bool ctrl_pressed  = false;
    bool alt_pressed   = false;

    Event_Type type    = EVENT_UNINITIALIZED;

    // If EVENT_KEYBOARD:
    u32      key_pressed;
    Key_Code key_code = CODE_UNKNOWN;
    bool     repeat = false;

    // If EVENT_TEXT_INPUT:
    u32 utf32;

    // If KEYBOARD event that also generated TEXT_INPUT events, this will tell
    // you how many TEXT_INPUT events after this KEYBOARD event were generated.
    u16 text_input_count;

    // Mouse events
    i32 typical_wheel_delta;
    i32 wheel_delta;
};

struct Wheel_Delta
{
    i32 vertical   = 0;
    i32 horizontal = 0;
};

// Per-frame mouse deltas:
extern i32         mouse_delta_x;
extern i32         mouse_delta_y;
extern Wheel_Delta mouse_wheel_delta;

extern RArr<Event> events_this_frame;
extern SArr<Key_Current_State> input_button_states;



void input_per_frame_event_and_flag_update();
void hook_our_input_event_system_to_glfw(GLFWwindow *window);
void update_linux_events();
void toggle_fullscreen();
void per_frame_update_mouse_position();

void hide_os_cursor(GLFWwindow *glfw_window);
void show_os_cursor(GLFWwindow *glfw_window);
void disable_os_cursor(GLFWwindow *glfw_window);

bool ui_button_was_pressed(Key_Code key_code);
bool ui_button_is_down(Key_Code key_code);
