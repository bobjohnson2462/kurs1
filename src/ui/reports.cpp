#include "ui/reports.h"

#include "osmod/depreciation.h"
#include "osmod/money.h"

#include <string>

namespace osmod {
namespace ui {
namespace {

size_t displayWidth(const std::string& text) {
    size_t width = 0;
    for (size_t i = 0; i < text.size();) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch < 0x80) {
            i += 1;
        } else if ((ch >> 5) == 0x6) {
            i += 2;
        } else if ((ch >> 4) == 0xE) {
            i += 3;
        } else {
            i += 4;
        }
        ++width;
    }
    return width;
}

std::string clip(const std::string& text, size_t width) {
    if (displayWidth(text) <= width) {
        return text;
    }
    std::string out;
    size_t used = 0;
    for (size_t i = 0; i < text.size() && used + 1 < width;) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        size_t step = ch < 0x80 ? 1 : ((ch >> 5) == 0x6 ? 2 : ((ch >> 4) == 0xE ? 3 : 4));
        out.append(text, i, step);
        i += step;
        ++used;
    }
    out += "…";
    return out;
}

std::string cellLeft(const std::string& text, size_t width) {
    std::string body = clip(text, width);
    size_t have = displayWidth(body);
    if (have < width) {
        body.append(width - have, ' ');
    }
    return body;
}

std::string cellRight(const std::string& text, size_t width) {
    std::string body = clip(text, width);
    size_t have = displayWidth(body);
    if (have < width) {
        body.insert(0, width - have, ' ');
    }
    return body;
}

}  // namespace

void printSummary(std::ostream& out, const Service& svc, const std::string& engine, const std::string& company,
                  const std::string& role) {
    Summary s = svc.summary();
    out << "\nСводка по фирме\n";
    out << "Фирма: " << (company.empty() ? "не задана" : company) << "\n";
    out << "Движок: " << engine << " | роль: " << role << "\n";
    out << "Карточек: " << s.totalAssets << " (в учёте " << s.activeAssets << ", списано " << s.retiredAssets
        << ")\n";
    out << "Основные средства: " << s.fixedAssets << " на " << formatKop(s.fixedCostKop) << " руб.\n";
    out << "Малоценки: " << s.lowValueAssets << " на " << formatKop(s.lowValueKop) << " руб.\n";
    out << "Первоначальная стоимость всего: " << formatKop(s.costKop) << " руб.\n";
    out << "Начислено: " << formatKop(s.accumulatedKop) << " руб.\n";
    out << "Балансовая: " << formatKop(s.bookKop) << " руб.\n";
    if (s.lastClosed) {
        out << "Последнее закрытие: " << periodText(*s.lastClosed) << "\n";
    } else {
        out << "Последнее закрытие: ещё не закрывали\n";
    }
}

void printPeople(std::ostream& out, Store& store) {
    out << "\nСправочник материально ответственных лиц\n";
    out << cellLeft("id", 5) << ' ' << cellLeft("ФИО", 34) << "Должность\n";
    for (const Person& p : store.people()) {
        out << cellLeft(std::to_string(p.id), 5) << ' ' << cellLeft(p.name, 34) << p.post << "\n";
    }
}

void printSheet(std::ostream& out, const Service& svc) {
    out << "\nВедомость остатков\n";
    out << cellLeft("Номер", 10) << ' ' << cellLeft("Наименование", 24) << ' ' << cellLeft("Способ", 20) << ' '
        << cellLeft("МОЛ", 16) << ' ' << cellRight("Первонач.", 12) << ' ' << cellRight("Начислено", 12) << ' '
        << cellRight("Балансовая", 12) << "  Состояние\n";
    for (const SheetLine& row : svc.sheet()) {
        out << cellLeft(row.inv, 10) << ' ' << cellLeft(row.title, 24) << ' ' << cellLeft(row.method, 20) << ' '
            << cellLeft(row.person, 16) << ' ' << cellRight(formatKop(row.costKop), 12) << ' '
            << cellRight(formatKop(row.accumulatedKop), 12) << ' ' << cellRight(formatKop(row.bookKop), 12) << "  "
            << row.status << "\n";
    }
}

void printCard(std::ostream& out, const Service& svc, int64_t assetId) {
    out << "\n" << svc.cardText(assetId);
}

void printCloseResult(std::ostream& out, const std::vector<CloseLine>& lines, bool showSkips) {
    int64_t total = 0;
    int charged = 0;
    int skipped = 0;
    for (const CloseLine& line : lines) {
        if (line.skipped || line.amountKop == 0) {
            ++skipped;
            if (showSkips) {
                out << cellLeft(line.inv, 10) << "  пропуск  " << line.note << "\n";
            }
            continue;
        }
        ++charged;
        total += line.amountKop;
        out << cellLeft(line.inv, 10) << "  " << cellRight(formatKop(line.amountKop), 13)
            << "  балансовая " << cellRight(formatKop(line.bookKop), 13) << "  " << line.note << "\n";
    }
    out << "Проводок: " << charged;
    if (showSkips) {
        out << ", пропусков: " << skipped;
    }
    out << "\n";
    out << "Итого начислено: " << formatKop(total) << " руб.\n";
}

void printJournal(std::ostream& out, const Service& svc, int limit) {
    out << "\nЖурнал действий, последние " << limit << " записей\n";
    for (const std::string& row : svc.journal(limit)) {
        out << row << "\n";
    }
}

void printForecast(std::ostream& out, const Forecast& forecast) {
    out << "\nПрогноз по " << forecast.inv << " " << forecast.title << "\n";
    out << "Способ: " << forecast.method << "\n";
    out << "Сейчас балансовая: " << formatKop(forecast.bookKop) << " руб., ниже "
        << formatKop(forecast.floorKop) << " руб. не уйду.\n";
    if (!forecast.note.empty()) {
        out << forecast.note << "\n";
    }
    if (forecast.lines.empty()) {
        out << "Начислять нечего.\n";
        return;
    }
    out << cellLeft("Месяц", 10) << ' ' << cellRight("Начислено", 13) << ' ' << cellRight("Балансовая", 13) << ' '
        << cellRight("Выработка", 11) << "  Комментарий\n";
    for (const ForecastLine& line : forecast.lines) {
        out << cellLeft(periodText(line.period), 10) << ' ' << cellRight(formatKop(line.amountKop), 13) << ' '
            << cellRight(formatKop(line.bookKop), 13) << ' ' << cellRight(std::to_string(line.units), 11) << "  "
            << line.note << "\n";
    }
    out << "Итого за горизонт: " << formatKop(forecast.totalKop) << " руб.";
    if (forecast.stoppedAtFloor) {
        out << " (дальше ликвидационная)";
    }
    out << "\n";
}

void printPeriodReport(std::ostream& out, const PeriodReport& report) {
    out << "\nОтчёт за " << periodText(report.period) << "\n";
    if (!report.closed) {
        out << "Месяц ещё не закрыт, проводок нет.\n";
        return;
    }
    out << cellLeft("Номер", 10) << ' ' << cellLeft("Наименование", 26) << ' ' << cellLeft("МОЛ", 18) << ' '
        << cellRight("Начислено", 12) << ' ' << cellRight("Балансовая", 12) << ' ' << cellRight("Выработка", 10)
        << "\n";
    for (const PeriodLine& line : report.lines) {
        out << cellLeft(line.inv, 10) << ' ' << cellLeft(line.title, 26) << ' ' << cellLeft(line.person, 18) << ' '
            << cellRight(formatKop(line.amountKop), 12) << ' ' << cellRight(formatKop(line.bookKop), 12) << ' '
            << cellRight(std::to_string(line.units), 10) << "\n";
    }
    out << "Проводок: " << report.lines.size() << ", выработка: " << report.totalUnits << "\n";
    out << "Итого: " << formatKop(report.totalKop) << " руб.\n";
}

void printYearTotals(std::ostream& out, const YearTotals& totals) {
    out << "\nИтоги за " << totals.year << " год\n";
    out << cellLeft("Месяц", 8) << ' ' << cellRight("Начислено", 14) << "\n";
    for (int month = 1; month <= 12; ++month) {
        std::string label = month < 10 ? "0" + std::to_string(month) : std::to_string(month);
        out << cellLeft(label, 8) << ' ' << cellRight(formatKop(totals.byMonth[month - 1]), 14) << "\n";
    }
    out << cellLeft("Итого", 8) << ' ' << cellRight(formatKop(totals.totalKop), 14) << "\n";
    out << "Закрытых месяцев: " << totals.monthsClosed << "\n";
}

void printMethods(std::ostream& out) {
    out << "Способы начисления амортизации:\n";
    for (int code = 1; code <= kMethodCount; ++code) {
        out << "  " << code << " — " << strategyFor(static_cast<Method>(code)).name() << "\n";
    }
}

}  // namespace ui
}  // namespace osmod
