#include "Input.h"

#include <ranges>

void Input::UpdateStates()
{
    for (auto& state : m_keys | std::views::values)
    {
        if (state == KeyEvent::Pressed)
        {
            state = KeyEvent::Down;
        }
        else if (state == KeyEvent::Released)
        {
            state = KeyEvent::Up;
        }
    }

    for (auto& state : m_mouseButtons | std::views::values)
    {
        if (state == KeyEvent::Pressed)
        {
            state = KeyEvent::Down;
        }
        else if (state == KeyEvent::Released)
        {
            state = KeyEvent::Up;
        }
    }
}

KeyEvent Input::GetKeyState(Key key) const
{
    auto it = m_keys.find(key);
    if (it == m_keys.end())
    {
        return KeyEvent::None;
    }
    return it->second;
}

KeyEvent Input::GetMouseButtonState(MouseButton button) const
{
    auto it = m_mouseButtons.find(button);
    if (it == m_mouseButtons.end())
    {
        return KeyEvent::None;
    }
    return it->second;
}

bool Input::IsKeyDown(Key key) const
{
    return GetKeyState(key) == KeyEvent::Down;
}

bool Input::IsKeyPressed(Key key) const
{
    return GetKeyState(key) == KeyEvent::Pressed;
}

bool Input::IsKeyReleased(Key key) const
{
    return GetKeyState(key) == KeyEvent::Released;
}

bool Input::IsKeyUp(Key key) const
{
    return GetKeyState(key) == KeyEvent::Up;
}

bool Input::IsMouseButtonDown(MouseButton button) const
{
    return GetMouseButtonState(button) == KeyEvent::Down;
}

bool Input::IsMouseButtonPressed(MouseButton button) const
{
    return GetMouseButtonState(button) == KeyEvent::Pressed;
}

bool Input::IsMouseButtonReleased(MouseButton button) const
{
    return GetMouseButtonState(button) == KeyEvent::Released;
}

bool Input::IsMouseButtonUp(MouseButton button) const
{
    return GetMouseButtonState(button) == KeyEvent::Up;
}
