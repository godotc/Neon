#pragma once

struct EventProcessState
{
    enum EResult
    {
        Handled = 0,
        Continue,
        ENUM_MAX
    };

    EResult result = Handled;

    EventProcessState(EResult result) : result(result) {}

    operator bool() const
    {
        return result == Handled;
    }

}; // namespace EventProcessState
