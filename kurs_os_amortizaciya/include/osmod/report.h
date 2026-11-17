#pragma once

#include "osmod/service.h"

#include <string>

namespace osmod {

// Деньги для CSV: без пробелов, с десятичной запятой.
std::string moneyCsv(int64_t kop);
// Поле экранируется кавычками, если внутри есть «;», кавычка или перевод строки.
std::string csvField(const std::string& text);

bool exportSheetCsv(const Service& svc, const std::string& path, std::string& error);
bool exportPeriodCsv(const PeriodReport& report, const std::string& path, std::string& error);

}  // namespace osmod
