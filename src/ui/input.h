#pragma once

#include "osmod/model.h"

#include <cstdint>
#include <string>

namespace osmod {
namespace ui {

// Консоль Windows переводим в UTF-8, иначе кириллица рассыпается.
void setupConsole();

std::string readLine();
std::string askText(const std::string& prompt, bool allowEmpty = false);
int askInt(const std::string& prompt, int low, int high);
int64_t askInt64(const std::string& prompt, int64_t low, int64_t high);
int64_t askMoneyKop(const std::string& prompt, bool allowZero);
DateYmd askDate(const std::string& prompt);
Method askMethod();
bool askYesNo(const std::string& prompt);

}  // namespace ui
}  // namespace osmod
