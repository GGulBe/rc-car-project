#include "TerminalInput.h"
#include "util.h"

TerminalInput::TerminalInput() {
    if (!::isatty(STDIN_FILENO)) throw std::runtime_error("Keyboard input requires a real terminal");
    if (::tcgetattr(STDIN_FILENO, &original_) < 0) throw systemError("tcgetattr failed");
    termios raw = original_;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) throw systemError("tcsetattr failed");
    active_ = true;
}

TerminalInput::~TerminalInput() {
    if (active_) ::tcsetattr(STDIN_FILENO, TCSANOW, &original_);
}

int TerminalInput::readKey(int timeoutMs) const {
    pollfd descriptor{};
    descriptor.fd = STDIN_FILENO;
    descriptor.events = POLLIN;
    const int result = ::poll(&descriptor, 1, timeoutMs);
    if (result < 0) {
        if (errno == EINTR) return -1;
        throw systemError("poll failed");
    }
    if (result == 0 || (descriptor.revents & POLLIN) == 0) return -1;
    unsigned char key = 0;
    return ::read(STDIN_FILENO, &key, 1) == 1 ? static_cast<int>(key) : -1;
}
