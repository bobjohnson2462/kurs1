#pragma once

#include <string>

namespace osmod {
namespace ui {

void printUsage();
int runInteractive(const std::string& connection = "");
int runDemo(const std::string& connection = "");

}  // namespace ui
}  // namespace osmod
