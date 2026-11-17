#include "osmod/money.h"

#include <cctype>
#include <cstdio>
#include <limits>
#include <vector>

namespace osmod {
namespace {

// Убираем пробелы, включая неразрывный (в UTF-8 это 0xC2 0xA0).
std::string withoutSpaces(const std::string& text) {
    std::string s;
    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (ch == ' ' || ch == '\t') {
            continue;
        }
        if (static_cast<unsigned char>(ch) == 0xC2 && i + 1 < text.size() &&
            static_cast<unsigned char>(text[i + 1]) == 0xA0) {
            ++i;
            continue;
        }
        s.push_back(ch);
    }
    return s;
}

}  // namespace

std::string formatKop(int64_t kop) {
    bool neg = kop < 0;
    if (neg) {
        kop = -kop;
    }
    int64_t rub = kop / 100;
    int frac = static_cast<int>(kop % 100);
    std::string digits = std::to_string(rub);
    std::string grouped;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        grouped.push_back(digits[static_cast<size_t>(i)]);
        ++count;
        if (count % 3 == 0 && i != 0) {
            grouped.push_back(' ');
        }
    }
    std::string body(grouped.rbegin(), grouped.rend());
    char tail[8];
    std::snprintf(tail, sizeof(tail), ",%02d", frac);
    return (neg ? "-" : "") + body + tail;
}

// Принимаем "120000", "1 200,50", "1.640.000 руб.", "5000 ₽" и похожие записи.
int64_t parseRubToKop(const std::string& text) {
    std::string s = withoutSpaces(text);

    size_t lastDigit = std::string::npos;
    for (size_t i = 0; i < s.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(s[i]))) {
            lastDigit = i;
        }
    }
    if (lastDigit == std::string::npos) {
        throw std::runtime_error("сумма задана не числом: " + text);
    }
    s = s.substr(0, lastDigit + 1);  // отрезаем "руб.", "р.", "₽"

    size_t i = 0;
    bool neg = false;
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
        neg = s[0] == '-';
        ++i;
    }
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i]))) {
        throw std::runtime_error("сумма задана не числом: " + text);
    }

    std::vector<std::string> groups(1);
    for (; i < s.size(); ++i) {
        char ch = s[i];
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            groups.back().push_back(ch);
        } else if (ch == '.' || ch == ',') {
            groups.emplace_back();
        } else {
            throw std::runtime_error("в сумме лишние символы: " + text);
        }
    }
    for (const std::string& g : groups) {
        if (g.empty()) {
            throw std::runtime_error("в сумме лишний разделитель: " + text);
        }
    }

    // Последняя группа из 1-2 цифр — копейки, остальные — тысячи по три цифры.
    int frac = 0;
    size_t rubGroups = groups.size();
    if (groups.size() > 1 && groups.back().size() <= 2) {
        const std::string& tail = groups.back();
        frac = (tail.size() == 1) ? (tail[0] - '0') * 10 : (tail[0] - '0') * 10 + (tail[1] - '0');
        rubGroups = groups.size() - 1;
    }
    for (size_t g = 1; g < rubGroups; ++g) {
        if (groups[g].size() != 3) {
            throw std::runtime_error("тысячи пишите по три цифры: " + text);
        }
    }

    int64_t rub = 0;
    const int64_t limit = std::numeric_limits<int64_t>::max() / 100;
    for (size_t g = 0; g < rubGroups; ++g) {
        for (char ch : groups[g]) {
            if (rub > limit / 10) {
                throw std::runtime_error("слишком большая сумма: " + text);
            }
            rub = rub * 10 + (ch - '0');
        }
    }
    int64_t kop = rub * 100 + frac;
    return neg ? -kop : kop;
}

int64_t mulDivFloor(int64_t a, int64_t b, int64_t d) {
    if (d <= 0) {
        throw std::runtime_error("делитель должен быть больше нуля");
    }
    if (a < 0 || b < 0) {
        throw std::runtime_error("отрицательные деньги в расчёте не жду");
    }
    if (b != 0 && a > (INT64_C(9223372036854775807) / b)) {
        int64_t q = a / d;
        int64_t r = a % d;
        if (b != 0 && q > (INT64_C(9223372036854775807) / b)) {
            throw std::runtime_error("переполнение при умножении суммы");
        }
        int64_t part = q * b;
        if (b != 0 && r > (INT64_C(9223372036854775807) / b)) {
            throw std::runtime_error("переполнение остатка");
        }
        return part + (r * b) / d;
    }
    return (a * b) / d;
}

}  // namespace osmod
