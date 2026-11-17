#include "osmod/model.h"

#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace osmod {

Period nextMonth(Period p) {
    if (p.month == 12) {
        return Period{p.year + 1, 1};
    }
    return Period{p.year, p.month + 1};
}

Period periodOf(DateYmd d) {
    return Period{d.year, d.month};
}

std::string periodText(Period p) {
    std::ostringstream os;
    os << (p.month < 10 ? "0" : "") << p.month << "." << p.year;
    return os.str();
}

std::string dateText(DateYmd d) {
    std::ostringstream os;
    os << (d.day < 10 ? "0" : "") << d.day << "." << (d.month < 10 ? "0" : "") << d.month << "." << d.year;
    return os.str();
}

bool validDate(DateYmd d) {
    if (d.year < 1990 || d.year > 2100 || d.month < 1 || d.month > 12 || d.day < 1) {
        return false;
    }
    static const int mdays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int dim = mdays[d.month];
    bool leap = (d.year % 4 == 0 && d.year % 100 != 0) || (d.year % 400 == 0);
    if (d.month == 2 && leap) {
        dim = 29;
    }
    return d.day <= dim;
}

DateYmd parseDateText(const std::string& text) {
    DateYmd d;
    int pos = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%d%n", &d.year, &d.month, &d.day, &pos) != 3 ||
        pos != static_cast<int>(text.size())) {
        pos = 0;
        if (std::sscanf(text.c_str(), "%d.%d.%d%n", &d.day, &d.month, &d.year, &pos) != 3 ||
            pos != static_cast<int>(text.size())) {
            throw std::runtime_error("дата должна быть ГГГГ-ММ-ДД или ДД.ММ.ГГГГ");
        }
    }
    if (!validDate(d)) {
        throw std::runtime_error("такой даты нет: " + text);
    }
    return d;
}

const char* kindName(AssetKind k) {
    return k == AssetKind::Fixed ? "основное средство" : "малоценка";
}

const char* statusName(AssetStatus s) {
    return s == AssetStatus::Active ? "в учёте" : "списано";
}

}  // namespace osmod
