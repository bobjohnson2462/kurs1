#include "ui/input.h"
#include "ui/menu.h"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    osmod::ui::setupConsole();
    try {
        std::string first = argc >= 2 ? argv[1] : "";
        if (first == "--help" || first == "-h") {
            osmod::ui::printUsage();
            return 0;
        }
        if (first == "--demo") {
            return osmod::ui::runDemo(argc >= 3 ? argv[2] : "");
        }
        if (!first.empty() && first[0] != '-') {
            return osmod::ui::runInteractive(first);
        }
        return osmod::ui::runInteractive();
    } catch (const std::exception& ex) {
        std::cerr << "Остановился: " << ex.what() << "\n";
        return 1;
    }
}
