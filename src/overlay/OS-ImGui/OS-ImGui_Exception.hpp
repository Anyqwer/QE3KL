#pragma once
#include <exception>
#include <string>

namespace OSImGui
{
    class OSException : public std::exception
    {
    private:
        std::string Message;
    public:
        OSException(const std::string& Msg) : Message(Msg) {}
        const char* what() const noexcept override { return Message.c_str(); }
    };
}
