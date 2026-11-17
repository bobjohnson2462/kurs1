#include "osmod/report.h"

#include "osmod/model.h"

#include <fstream>
#include <vector>

namespace osmod {
namespace {

bool writeCsv(const std::string& path, const std::string& header, const std::vector<std::string>& rows,
              std::string& error) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error = "не открыл файл " + path;
        return false;
    }
    // BOM нужен Excel, иначе кириллица открывается кракозябрами.
    file << "\xEF\xBB\xBF" << header << "\r\n";
    for (const std::string& row : rows) {
        file << row << "\r\n";
    }
    if (!file) {
        error = "запись в " + path + " оборвалась";
        return false;
    }
    return true;
}

}  // namespace

std::string moneyCsv(int64_t kop) {
    std::string sign = kop < 0 ? "-" : "";
    if (kop < 0) {
        kop = -kop;
    }
    int64_t rub = kop / 100;
    int frac = static_cast<int>(kop % 100);
    std::string tail = frac < 10 ? "0" + std::to_string(frac) : std::to_string(frac);
    return sign + std::to_string(rub) + "," + tail;
}

std::string csvField(const std::string& text) {
    if (text.find_first_of(";\"\r\n") == std::string::npos) {
        return text;
    }
    std::string out = "\"";
    for (char ch : text) {
        if (ch == '"') {
            out += "\"\"";
        } else {
            out += ch;
        }
    }
    out += "\"";
    return out;
}

bool exportSheetCsv(const Service& svc, const std::string& path, std::string& error) {
    std::vector<std::string> rows;
    for (const SheetLine& row : svc.sheet()) {
        rows.push_back(csvField(row.inv) + ";" + csvField(row.title) + ";" + csvField(row.method) + ";" +
                       moneyCsv(row.costKop) + ";" + moneyCsv(row.accumulatedKop) + ";" +
                       moneyCsv(row.bookKop) + ";" + csvField(row.person) + ";" + csvField(row.status));
    }
    return writeCsv(path,
                    "Номер;Наименование;Способ;Первоначальная;Начислено;Балансовая;МОЛ;Состояние", rows, error);
}

bool exportPeriodCsv(const PeriodReport& report, const std::string& path, std::string& error) {
    std::vector<std::string> rows;
    for (const PeriodLine& line : report.lines) {
        rows.push_back(periodText(report.period) + ";" + csvField(line.inv) + ";" + csvField(line.title) + ";" +
                       csvField(line.person) + ";" + moneyCsv(line.amountKop) + ";" + moneyCsv(line.bookKop) + ";" +
                       std::to_string(line.units));
    }
    return writeCsv(path, "Месяц;Номер;Наименование;МОЛ;Начислено;Балансовая;Выработка", rows, error);
}

}  // namespace osmod
