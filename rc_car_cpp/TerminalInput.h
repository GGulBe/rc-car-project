#pragma once

#include <termios.h>
#include <poll.h>
#include <unistd.h>
#include <stdexcept>
#include <cerrno>

class TerminalInput {
public:
    TerminalInput();
    ~TerminalInput();

    TerminalInput(const TerminalInput&) = delete;
    TerminalInput& operator=(const TerminalInput&) = delete;

    int readKey(int timeoutMs = 0) const;

private:
    termios original_{};
    bool active_ = false;
};