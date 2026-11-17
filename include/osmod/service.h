#pragma once

#include "osmod/store.h"

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace osmod {

struct CloseLine {
    std::string inv;
    std::string title;
    int64_t amountKop = 0;
    int64_t bookKop = 0;
    bool skipped = false;
    std::string note;
};

struct SheetLine {
    std::string inv;
    std::string title;
    std::string method;
    int64_t costKop = 0;
    int64_t accumulatedKop = 0;
    int64_t bookKop = 0;
    std::string person;
    std::string status;
};

// Сводка по фирме для главного экрана.
struct Summary {
    int totalAssets = 0;
    int activeAssets = 0;
    int retiredAssets = 0;
    int fixedAssets = 0;
    int lowValueAssets = 0;
    int64_t costKop = 0;
    int64_t fixedCostKop = 0;
    int64_t lowValueKop = 0;
    int64_t accumulatedKop = 0;
    int64_t bookKop = 0;
    std::optional<Period> lastClosed;
};

struct PeriodLine {
    std::string inv;
    std::string title;
    std::string person;
    int64_t amountKop = 0;
    int64_t bookKop = 0;
    int64_t units = 0;
};

struct PeriodReport {
    Period period{};
    bool closed = false;
    std::vector<PeriodLine> lines;
    int64_t totalKop = 0;
    int64_t totalUnits = 0;
};

struct YearTotals {
    int year = 0;
    std::array<int64_t, 12> byMonth{};
    int64_t totalKop = 0;
    int monthsClosed = 0;
};

struct ForecastLine {
    Period period{};
    int64_t amountKop = 0;
    int64_t bookKop = 0;
    int64_t units = 0;
    std::string note;
};

// Прогноз без записи в базу: считает те же стратегии, что и закрытие.
struct Forecast {
    std::string inv;
    std::string title;
    std::string method;
    std::string note;
    int64_t bookKop = 0;
    int64_t floorKop = 0;
    int64_t totalKop = 0;
    bool stoppedAtFloor = false;
    std::vector<ForecastLine> lines;
};

class Service {
public:
    explicit Service(Store& store);

    void seedDemo(const std::string& user);
    int64_t acceptAsset(const Asset& draft, const std::string& user);
    std::vector<CloseLine> closeMonth(Period p, const std::map<int64_t, int64_t>& unitsByAsset,
                                       const std::string& user);
    void retire(int64_t assetId, DateYmd date, const std::string& reason, const std::string& user);
    std::vector<SheetLine> sheet() const;
    std::string cardText(int64_t assetId) const;
    int64_t valueLimitKop() const;

    Summary summary() const;
    std::optional<Period> lastClosedPeriod() const;
    PeriodReport periodReport(Period p) const;
    YearTotals yearTotals(int year) const;
    Forecast forecast(int64_t assetId, int months, int64_t unitsPerMonth = 0) const;
    Period reopenLastMonth(const std::string& user);
    std::vector<std::string> journal(int limit) const;

private:
    Store& store_;
    void ensurePolicy();
};

}  // namespace osmod
