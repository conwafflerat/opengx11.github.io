#pragma once
#include <SDL.h>
#include <cstdint>

// Genesis controller button flags
enum Button : uint16_t {
    BTN_UP    = 1 << 0,
    BTN_DOWN  = 1 << 1,
    BTN_LEFT  = 1 << 2,
    BTN_RIGHT = 1 << 3,
    BTN_A     = 1 << 4,
    BTN_B     = 1 << 5,
    BTN_C     = 1 << 6,
    BTN_START = 1 << 7,
};

// In s2disasm the controller state lives at $FFFFF604/$FFFFF606 (held/pressed).
// Here we keep it as two bitfields identical to the original layout.
struct ControllerState {
    uint16_t held    = 0;   // currently pressed buttons
    uint16_t pressed = 0;   // newly pressed this frame (cleared each frame)
};

class Input {
public:
    void process_event(const SDL_Event &ev);
    void end_frame();   // promote pressed bits, clear one-shot flags

    const ControllerState &p1() const { return m_p1; }
    const ControllerState &p2() const { return m_p2; }

    void open_controller();
    void close_controller();

private:
    ControllerState m_p1, m_p2;
    uint16_t        m_new_p1 = 0;
    SDL_GameController *m_ctrl = nullptr;

    static uint16_t sdl_key_to_button(SDL_Keycode k);
    static uint16_t sdl_pad_to_button(SDL_GameControllerButton b);
};
