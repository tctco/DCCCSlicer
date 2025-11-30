#include "Logging.h"

#include <iostream>

namespace refactorCommon::log {

void debug(const std::string& message) {
    std::cout << message << std::endl;
}

}  // namespace refactorCommon::log


