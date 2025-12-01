#include "Logging.h"

#include <iostream>

namespace Common::log {

void debug(const std::string& message) {
    std::cout << message << std::endl;
}

}  // namespace Common::log


