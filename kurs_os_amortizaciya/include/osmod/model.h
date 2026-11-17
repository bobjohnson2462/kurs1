#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace osmod {

enum class Method { Linear = 1, Declining = 2, Output = 3, SumOfYears = 4 };
enum class AssetKind { Fixed = 1, LowValue = 2 };
enum class AssetStatus { Active = 1, Retired = 2 };

struct DateYmd {
    int year = 0;
    int month = 0;
    int day = 0;
};

struct Period {
    int year = 0;
    int month = 0;
    bool operator<(const Period& o) const {
        return year < o.year || (year == o.year && month < o.month);
    }
    bool operator==(const Period& o) const { return year == o.year && month == o.month; }
    bool operator<=(const Period& o) const { return *this < o || *this == o; }
};

Period nextMonth(Period p);
Period periodOf(DateYmd d);
std::string periodText(Period p);
std::string dateText(DateYmd d);
bool validDate(DateYmd d);
// Принимает ГГГГ-ММ-ДД и ДД.ММ.ГГГГ, проверяет календарь.
DateYmd parseDateText(const std::string& text);

struct Person {
    int64_t id = 0;
    std::string name;
    std::string post;
};

struct Asset {
    int64_t id = 0;
    std::string inv;
    std::string title;
    int64_t personId = 0;
    AssetKind kind = AssetKind::Fixed;
    AssetStatus status = AssetStatus::Active;
    Method method = Method::Linear;
    DateYmd accepted{};
    int64_t costKop = 0;
    int64_t liqKop = 0;
    int lifeMonths = 0;
    int declineX100 = 200;
    int64_t planUnits = 0;
    std::string place;
    std::string note;
    std::string retiredOn;
    std::string retireReason;
};

struct Charge {
    int64_t id = 0;
    int64_t assetId = 0;
    Period period{};
    int64_t amountKop = 0;
    int64_t bookKop = 0;
    int64_t units = 0;
};

const char* kindName(AssetKind k);
const char* statusName(AssetStatus s);

}  // namespace osmod
