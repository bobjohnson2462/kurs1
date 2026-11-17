#include "ui/input.h"

#include "osmod/depreciation.h"
#include "osmod/money.h"

#include <cctype>
#include <iostream>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace osmod {
namespace ui {
namespace {

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.pop_back();
    }
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    return s.substr(i);
}

int64_t wholeFromText(const std::string& text, int64_t low, int64_t high) {
    size_t used = 0;
    long long value = std::stoll(text, &used);
    while (used < text.size() && std::isspace(static_cast<unsigned char>(text[used]))) {
        ++used;
    }
    if (used != text.size()) {
        throw std::runtime_error("в числе лишние символы");
    }
    if (value < low || value > high) {
        throw std::runtime_error("число вне допустимого диапазона");
    }
    return static_cast<int64_t>(value);
}

}  // namespace

void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::string readLine() {
    std::string s;
    if (!std::getline(std::cin, s)) {
        throw std::runtime_error("ввод закончился, дальше спрашивать некого");
    }
    return s;
}

std::string askText(const std::string& prompt, bool allowEmpty) {
    while (true) {
        std::cout << prompt;
        std::cout.flush();
        std::string text = trim(readLine());
        if (!text.empty() || allowEmpty) {
            return text;
        }
        std::cout << "Пустое значение здесь не подходит, повторите.\n";
    }
}

int askInt(const std::string& prompt, int low, int high) {
    while (true) {
        std::string text = askText(prompt + " (" + std::to_string(low) + ".." + std::to_string(high) + "): ");
        try {
            return static_cast<int>(wholeFromText(text, low, high));
        } catch (const std::exception&) {
            std::cout << "Нужно целое число от " << low << " до " << high << ".\n";
        }
    }
}

int64_t askInt64(const std::string& prompt, int64_t low, int64_t high) {
    while (true) {
        std::string text = askText(prompt);
        try {
            return wholeFromText(text, low, high);
        } catch (const std::exception&) {
            std::cout << "Нужно целое число от " << low << " до " << high << ".\n";
        }
    }
}

int64_t askMoneyKop(const std::string& prompt, bool allowZero) {
    while (true) {
        std::string text = askText(prompt);
        try {
            int64_t kop = parseRubToKop(text);
            if (kop < 0) {
                throw std::runtime_error("сумма не бывает отрицательной");
            }
            if (kop == 0 && !allowZero) {
                throw std::runtime_error("ноль здесь не подходит");
            }
            return kop;
        } catch (const std::exception& ex) {
            std::cout << "Сумму не понял: " << ex.what() << "\n";
            std::cout << "Примеры: 186400, 186 400, 186400,50, 186400.50 руб.\n";
        }
    }
}

DateYmd askDate(const std::string& prompt) {
    while (true) {
        std::string text = askText(prompt + " (ГГГГ-ММ-ДД или ДД.ММ.ГГГГ): ");
        try {
            return parseDateText(text);
        } catch (const std::exception& ex) {
            std::cout << "Дату не принял: " << ex.what() << "\n";
        }
    }
}

Method askMethod() {
    std::cout << "Способы начисления:\n";
    for (int code = 1; code <= kMethodCount; ++code) {
        std::cout << "  " << code << " — " << strategyFor(static_cast<Method>(code)).name() << "\n";
    }
    return static_cast<Method>(askInt("Номер способа", 1, kMethodCount));
}

bool askYesNo(const std::string& prompt) {
    while (true) {
        std::string text = askText(prompt + " (да/нет): ");
        if (text == "да" || text == "Да" || text == "ДА" || text == "д" || text == "Д" || text == "y" ||
            text == "Y" || text == "yes") {
            return true;
        }
        if (text == "нет" || text == "Нет" || text == "НЕТ" || text == "н" || text == "Н" || text == "n" ||
            text == "N" || text == "no") {
            return false;
        }
        std::cout << "Ответьте «да» или «нет».\n";
    }
}

}  // namespace ui
}  // namespace osmod
