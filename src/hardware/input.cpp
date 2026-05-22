#include "input.hpp"
#include <cstring>

uint16_t Input::sdl_key_to_button(SDL_Keycode k)
{
    switch (k) {
    case SDLK_UP:     return BTN_UP;
    case SDLK_DOWN:   return BTN_DOWN;
    case SDLK_LEFT:   return BTN_LEFT;
    case SDLK_RIGHT:  return BTN_RIGHT;
    case SDLK_z:      return BTN_A;
    case SDLK_x:      return BTN_B;
    case SDLK_c:      return BTN_C;
    case SDLK_RETURN: return BTN_START;
    default: return 0;
    }
}

uint16_t Input::sdl_pad_to_button(SDL_GameControllerButton b)
{
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:    return BTN_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return BTN_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return BTN_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return BTN_RIGHT;
    case SDL_CONTROLLER_BUTTON_X:          return BTN_A;
    case SDL_CONTROLLER_BUTTON_A:          return BTN_B;
    case SDL_CONTROLLER_BUTTON_B:          return BTN_C;
    case SDL_CONTROLLER_BUTTON_START:      return BTN_START;
    default: return 0;
    }
}

void Input::open_controller()
{
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            m_ctrl = SDL_GameControllerOpen(i);
            if (m_ctrl) break;
        }
    }
}

void Input::close_controller()
{
    if (m_ctrl) { SDL_GameControllerClose(m_ctrl); m_ctrl = nullptr; }
}

void Input::process_event(const SDL_Event &ev)
{
    switch (ev.type) {
    case SDL_KEYDOWN:
        m_new_p1 |= sdl_key_to_button(ev.key.keysym.sym);
        break;
    case SDL_KEYUP:
        m_new_p1 &= ~sdl_key_to_button(ev.key.keysym.sym);
        m_p1.held &= ~sdl_key_to_button(ev.key.keysym.sym);
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        m_new_p1 |= sdl_pad_to_button((SDL_GameControllerButton)ev.cbutton.button);
        break;
    case SDL_CONTROLLERBUTTONUP:
        m_new_p1 &= ~sdl_pad_to_button((SDL_GameControllerButton)ev.cbutton.button);
        m_p1.held &= ~sdl_pad_to_button((SDL_GameControllerButton)ev.cbutton.button);
        break;
    }
}

void Input::end_frame()
{
    // pressed = buttons that are down now but weren't last frame
    m_p1.pressed = m_new_p1 & ~m_p1.held;
    m_p1.held    = m_new_p1;
}
