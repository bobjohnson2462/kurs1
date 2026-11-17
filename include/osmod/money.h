#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace osmod {

std::string formatKop(int64_t kop);
int64_t parseRubToKop(const std::string& text);
int64_t mulDivFloor(int64_t a, int64_t b, int64_t d);

}  // namespace osmod
