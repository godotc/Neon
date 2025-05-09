#include "InputManager.h"
#include "../Log.h"

#include <SDL3/SDL.h>

#include <SDL3/SDL_mouse.h>

InputManager::InputManager()
{
    // Initialize mouse position
    float x, y;
    SDL_GetMouseState(&x, &y);
    mousePosition         = {static_cast<float>(x), static_cast<float>(y)};
    previousMousePosition = mousePosition;
}

void InputManager::update()
{
    // Store previous states
    previousKeyStates   = currentKeyStates;
    previousMouseStates = currentMouseStates;

    // Update mouse delta
    previousMousePosition = mousePosition;
    float x, y;
    SDL_GetMouseState(&x, &y);
    mousePosition = {static_cast<float>(x), static_cast<float>(y)};
    mouseDelta    = mousePosition - previousMousePosition;
}

EventProcessState InputManager::processEvent(const SDL_Event &event)
{
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
        currentKeyStates[event.key.key] = KeyState::Pressed;
        break;

    case SDL_EVENT_KEY_UP:
        currentKeyStates[event.key.key] = KeyState::Released;
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        currentMouseStates[event.button.button] = KeyState::Pressed;
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        currentMouseStates[event.button.button] = KeyState::Released;
        break;
    }
    return EventProcessState::Continue;
}

bool InputManager::isKeyPressed(SDL_Keycode keycode) const
{
    auto it = currentKeyStates.find(keycode);
    return it != currentKeyStates.end() && it->second == KeyState::Pressed;
}

bool InputManager::wasKeyPressed(SDL_Keycode keycode) const
{
    auto curr = currentKeyStates.find(keycode);
    auto prev = previousKeyStates.find(keycode);

    return (curr != currentKeyStates.end() && curr->second == KeyState::Pressed) &&
           (prev == previousKeyStates.end() || prev->second == KeyState::Released);
}

bool InputManager::wasKeyReleased(SDL_Keycode keycode) const
{
    auto curr = currentKeyStates.find(keycode);
    auto prev = previousKeyStates.find(keycode);

    return (curr != currentKeyStates.end() && curr->second == KeyState::Released) &&
           (prev != previousKeyStates.end() && prev->second == KeyState::Pressed);
}

bool InputManager::isMouseButtonPressed(Uint8 button) const
{
    auto it = currentMouseStates.find(button);
    return it != currentMouseStates.end() && it->second == KeyState::Pressed;
}

bool InputManager::wasMouseButtonPressed(Uint8 button) const
{
    auto curr = currentMouseStates.find(button);
    auto prev = previousMouseStates.find(button);

    return (curr != currentMouseStates.end() && curr->second == KeyState::Pressed) &&
           (prev == previousMouseStates.end() || prev->second == KeyState::Released);
}

bool InputManager::wasMouseButtonReleased(Uint8 button) const
{
    auto curr = currentMouseStates.find(button);
    auto prev = previousMouseStates.find(button);

    return (curr != currentMouseStates.end() && curr->second == KeyState::Released) &&
           (prev != previousMouseStates.end() && prev->second == KeyState::Pressed);
}
