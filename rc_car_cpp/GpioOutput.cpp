#include "GpioOutput.h"
#include "util.h"


GpioOutput::GpioOutput(unsigned int offset, bool inoutFlag) {
    chipFd_ = ::open("/dev/gpiochip0", O_RDONLY | O_CLOEXEC);
    if (chipFd_ < 0) throw systemError("Failed to open /dev/gpiochip0");

    gpio_v2_line_request request{};
    request.offsets[0] = offset;
    request.num_lines = 1;
    request.config.flags = (inoutFlag == 1 ? GPIO_V2_LINE_FLAG_INPUT : GPIO_V2_LINE_FLAG_OUTPUT); // TOOD: 생성자 매개변수 읽기 쓰기 모드 변경 완료
    std::strncpy(request.consumer, consumer.c_str(), sizeof(request.consumer) - 1);

    if (::ioctl(chipFd_, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
        ::close(chipFd_);
        chipFd_ = -1;
        throw systemError("Failed to request GPIO output");
    }

    lineFd_ = request.fd;
    if (outputMode_) setLevel(false);
}

GpioOutput::~GpioOutput() {
    if (lineFd_ >= 0) ::close(lineFd_);
    if (chipFd_ >= 0) ::close(chipFd_);
}


void GpioOutput::setLevel(bool value) { // 해당 핀 밸류에 전압 조정 메소드
    gpio_v2_line_values values{};
    values.mask = 1;
    values.bits = value ? 1 : 0;
    if (::ioctl(lineFd_, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) throw systemError("Failed to set GPIO output");
}
