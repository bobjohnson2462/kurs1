#include "osmod/depreciation.h"
#include "osmod/money.h"
#include "osmod/report.h"
#include "osmod/service.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

int fails = 0;

void expect(bool ok, const std::string& text) {
    if (!ok) {
        std::cerr << "FAIL " << text << "\n";
        ++fails;
    }
}

template <typename Fn>
void expectThrows(Fn fn, const std::string& text) {
    try {
        fn();
    } catch (const std::exception&) {
        return;
    }
    expect(false, text);
}

std::string readAll(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

osmod::Asset makeAsset(int64_t personId, const std::string& inv, int64_t costKop, int64_t liqKop, int lifeMonths,
                       osmod::Method method, osmod::DateYmd accepted = {2024, 1, 15}) {
    osmod::Asset a;
    a.inv = inv;
    a.title = "Тестовый объект " + inv;
    a.personId = personId;
    a.costKop = costKop;
    a.liqKop = liqKop;
    a.lifeMonths = lifeMonths;
    a.method = method;
    a.accepted = accepted;
    a.place = "тест";
    return a;
}

std::string testConnection() {
    const char* preset = std::getenv("OSMOD_TEST_CONNECTION");
    if (preset != nullptr && preset[0] != '\0') {
        return preset;
    }
    return "Driver={ODBC Driver 17 for SQL Server};Server=localhost;Database=AssetOsTest;Trusted_Connection=yes;";
}

std::unique_ptr<osmod::Store> openTestStore() {
    auto store = osmod::openSqlServer(testConnection());
    store->migrate();
    store->resetAll();
    return store;
}

void testMoney() {
    expect(osmod::parseRubToKop("186400") == 18640000, "сумма без разделителей");
    expect(osmod::parseRubToKop("186 400") == 18640000, "пробелы в сумме");
    expect(osmod::parseRubToKop("186400,50") == 18640050, "запятая как десятичная");
    expect(osmod::parseRubToKop("186400.5") == 18640050, "точка как десятичная");
    expect(osmod::parseRubToKop("1 864,5") == 186450, "одна цифра после запятой");
    expect(osmod::parseRubToKop("1.640.000 руб.") == 164000000, "точки как тысячи и суффикс");
    expect(osmod::parseRubToKop("5000 \xE2\x82\xBD") == 500000, "знак рубля отрезается");
    expect(osmod::parseRubToKop("1\xC2\xA0" "864,00") == 186400, "неразрывный пробел");
    expect(osmod::parseRubToKop("-12,34") == -1234, "отрицательная сумма разбирается");
    expect(osmod::parseRubToKop("0") == 0, "ноль разбирается");
    expectThrows([] { osmod::parseRubToKop(""); }, "пустая сумма не проходит");
    expectThrows([] { osmod::parseRubToKop("abc"); }, "буквы вместо суммы не проходят");
    expectThrows([] { osmod::parseRubToKop("12,34,56"); }, "две десятичные группы не проходят");

    expect(osmod::formatKop(123456789) == "1 234 567,89", "формат с пробелами и копейками");
    expect(osmod::formatKop(-50) == "-0,50", "формат отрицательной суммы");
    expect(osmod::mulDivFloor(7, 3, 2) == 10, "mulDivFloor округляет вниз");
}

void testDates() {
    osmod::DateYmd leap = osmod::parseDateText("2024-02-29");
    expect(leap.year == 2024 && leap.month == 2 && leap.day == 29, "29 февраля високосного года");
    osmod::DateYmd dots = osmod::parseDateText("29.02.2024");
    expect(dots.year == 2024 && dots.month == 2 && dots.day == 29, "дата через точки");
    expectThrows([] { osmod::parseDateText("2023-02-29"); }, "29 февраля невисокосного года");
    expectThrows([] { osmod::parseDateText("31.04.2024"); }, "30 апреля не бывает");
    expectThrows([] { osmod::parseDateText("2024-02-29 лишнее"); }, "хвост после даты");
}

void testLinear() {
    osmod::DepreciationInput in;
    in.costKop = 100;
    in.liqKop = 0;
    in.lifeMonths = 3;
    in.method = osmod::Method::Linear;
    int64_t acc = 0;
    int64_t parts[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        in.accumulatedKop = acc;
        in.monthsAlready = i;
        osmod::DepreciationOutput out = osmod::chargeDepreciation(in);
        parts[i] = out.amountKop;
        acc += out.amountKop;
    }
    expect(parts[0] == 33 && parts[1] == 33 && parts[2] == 34, "хвост копеек 33+33+34");
    expect(acc == 100, "линейный итог сходится с базой");

    osmod::DepreciationInput comp;
    comp.costKop = osmod::parseRubToKop("186400");
    comp.liqKop = osmod::parseRubToKop("6400");
    comp.lifeMonths = 24;
    comp.method = osmod::Method::Linear;
    expect(osmod::chargeDepreciation(comp).amountKop == osmod::parseRubToKop("7500"),
           "компрессор 7500 в обычный месяц");
}

void testDeclining() {
    osmod::DepreciationInput gazelle;
    gazelle.costKop = osmod::parseRubToKop("1640000");
    gazelle.liqKop = osmod::parseRubToKop("240000");
    gazelle.lifeMonths = 60;
    gazelle.declineX100 = 200;
    gazelle.method = osmod::Method::Declining;
    expect(osmod::chargeDepreciation(gazelle).amountKop == osmod::parseRubToKop("54666,66"),
           "газель 04.2024 даёт 54 666,66");

    osmod::DepreciationInput in;
    in.costKop = 100000;
    in.liqKop = 90000;
    in.lifeMonths = 10;
    in.declineX100 = 200;
    in.method = osmod::Method::Declining;
    int64_t acc = 0;
    bool belowFloor = false;
    for (int i = 0; i < 10; ++i) {
        in.accumulatedKop = acc;
        in.monthsAlready = i;
        osmod::DepreciationOutput out = osmod::chargeDepreciation(in);
        if (out.bookKop < in.liqKop) {
            belowFloor = true;
        }
        acc += out.amountKop;
    }
    expect(!belowFloor, "ниже ликвидационной не ушли");
    expect(acc == 10000, "списали ровно амортизируемую часть");
}

void testOutput() {
    osmod::DepreciationInput in;
    in.costKop = osmod::parseRubToKop("510000");
    in.liqKop = osmod::parseRubToKop("30000");
    in.planUnits = 12000;
    in.lifeMonths = 48;
    in.method = osmod::Method::Output;
    in.unitsThisPeriod = 160;
    expect(osmod::chargeDepreciation(in).amountKop == osmod::parseRubToKop("6400"),
           "люлька за 160 часов даёт 6400");

    in.accumulatedKop = 48000000 - 50;
    in.accumulatedUnits = 11999;
    in.unitsThisPeriod = 1;
    expect(osmod::chargeDepreciation(in).amountKop == 50, "на последней единице плана забираю хвост");
}

void testSumOfYears() {
    const int64_t base = 6000000;
    osmod::DepreciationInput in;
    in.costKop = base;
    in.liqKop = 0;
    in.lifeMonths = 60;
    in.method = osmod::Method::SumOfYears;
    int64_t acc = 0;
    int64_t first = 0;
    int64_t previous = base;
    bool decreasing = true;
    for (int m = 0; m < 60; ++m) {
        in.accumulatedKop = acc;
        in.monthsAlready = m;
        osmod::DepreciationOutput out = osmod::chargeDepreciation(in);
        if (m == 0) {
            first = out.amountKop;
        }
        if (out.amountKop > previous) {
            decreasing = false;
        }
        previous = out.amountKop;
        acc += out.amountKop;
    }
    expect(first == 196721, "первый месяц по сумме чисел лет 1967,21");
    expect(acc == base, "сумма чисел лет закрывает базу ровно");
    expect(decreasing, "сумма чисел лет убывает");
    expect(std::string(osmod::strategyFor(osmod::Method::SumOfYears).name()) == "по сумме чисел лет",
           "у четвёртого способа своё имя");
}

void testRejectsBadInput() {
    osmod::DepreciationInput in;
    in.costKop = 10;
    in.liqKop = 11;
    in.lifeMonths = 2;
    in.method = osmod::Method::Linear;
    expectThrows([&in] { osmod::chargeDepreciation(in); }, "ликвидационная выше стоимости не проходит");

    osmod::DepreciationInput output;
    output.costKop = 1000;
    output.planUnits = 0;
    output.lifeMonths = 10;
    output.method = osmod::Method::Output;
    output.unitsThisPeriod = 1;
    expectThrows([&output] { osmod::chargeDepreciation(output); }, "выработка без плана не проходит");

    osmod::DepreciationInput units;
    units.costKop = 1000;
    units.planUnits = 10;
    units.lifeMonths = 10;
    units.method = osmod::Method::Output;
    units.unitsThisPeriod = 11;
    expectThrows([&units] { osmod::chargeDepreciation(units); }, "выработка больше плана не проходит");
}

void testDemoTotals() {
    auto store = openTestStore();
    osmod::Service svc(*store);
    svc.seedDemo("test");

    std::map<osmod::Period, int64_t> hours{
        {{2024, 7}, 160},  {{2024, 8}, 210}, {{2024, 9}, 40},   {{2024, 10}, 180}, {{2024, 11}, 90},
        {{2024, 12}, 20},  {{2025, 1}, 0},   {{2025, 2}, 150},  {{2025, 3}, 175},  {{2025, 4}, 200},
    };
    auto cradle = store->assetByInv("ОС-0003");
    expect(cradle.has_value(), "люлька заведена демо-данными");
    if (!cradle) {
        return;
    }

    for (int i = 0; i < 13; ++i) {
        osmod::Period p = osmod::nextMonth({2024, 3});
        int cursor = i;
        while (cursor-- > 0) {
            p = osmod::nextMonth(p);
        }
        std::map<int64_t, int64_t> units;
        auto found = hours.find(p);
        if (found != hours.end()) {
            units[cradle->id] = found->second;
        }
        svc.closeMonth(p, units, "test");
    }

    osmod::Summary summary = svc.summary();
    expect(summary.totalAssets == 5, "в демо пять карточек");
    expect(summary.fixedAssets == 3 && summary.lowValueAssets == 2, "три ОС и две малоценки");
    expect(summary.fixedCostKop == 233640000, "основные средства стоят 2 336 400,00");
    expect(summary.lowValueKop == 8599000, "малоценки стоят 85 990,00");
    expect(summary.accumulatedKop == 65604060, "за 13 месяцев начислено 656 040,60");
    expect(summary.lastClosed.has_value() && *summary.lastClosed == osmod::Period{2025, 4},
           "последнее закрытие 04.2025");

    osmod::PeriodReport april = svc.periodReport({2025, 4});
    expect(april.closed && april.lines.size() == 3, "в апреле три проводки");
    expect(april.totalKop > 0, "апрельский итог положительный");

    osmod::YearTotals y2024 = svc.yearTotals(2024);
    osmod::YearTotals y2025 = svc.yearTotals(2025);
    expect(y2024.monthsClosed == 9, "в 2024 закрыто девять месяцев");
    expect(y2025.monthsClosed == 4, "в 2025 закрыто четыре месяца");
    expect(y2024.totalKop + y2025.totalKop == summary.accumulatedKop, "годовые итоги сходятся с накопленным");

    auto comp = store->assetByInv("ОС-0002");
    expect(comp.has_value(), "компрессор на месте");
    if (comp) {
        size_t before = store->charges(comp->id).size();
        osmod::Forecast forecast = svc.forecast(comp->id, 6);
        expect(forecast.lines.size() == 6, "прогноз на шесть месяцев");
        expect(forecast.totalKop == 4500000, "прогноз компрессора 45 000,00");
        expect(store->charges(comp->id).size() == before, "прогноз не пишет проводки");
    }

    std::string error;
    expect(osmod::exportSheetCsv(svc, "data/test-vedomost.csv", error), "ведомость выгружается в CSV");
    std::string sheetCsv = readAll("data/test-vedomost.csv");
    expect(sheetCsv.size() > 3 && static_cast<unsigned char>(sheetCsv[0]) == 0xEF &&
               static_cast<unsigned char>(sheetCsv[1]) == 0xBB && static_cast<unsigned char>(sheetCsv[2]) == 0xBF,
           "CSV начинается с BOM");
    expect(sheetCsv.find("ОС-0001") != std::string::npos, "в CSV есть инвентарный номер");
    expect(sheetCsv.find(';') != std::string::npos, "в CSV разделитель точка с запятой");

    expect(osmod::exportPeriodCsv(april, "data/test-period.csv", error), "отчёт за месяц выгружается");
    std::string periodCsv = readAll("data/test-period.csv");
    expect(periodCsv.find("04.2025") != std::string::npos, "в CSV отчёта есть месяц");

    int64_t before = svc.summary().accumulatedKop;
    osmod::Period reopened = svc.reopenLastMonth("test");
    expect(reopened == osmod::Period{2025, 4}, "отменился последний месяц");
    expect(!svc.periodReport(reopened).closed, "после отмены месяц не закрыт");
    expect(svc.summary().accumulatedKop < before, "после отмены начислено меньше");

    std::map<int64_t, int64_t> again{{cradle->id, 200}};
    svc.closeMonth(reopened, again, "test");
    expect(svc.summary().accumulatedKop == before, "повторное закрытие вернуло прежний итог");
    expectThrows([&svc] { svc.closeMonth({2025, 4}, {}, "test"); }, "повтор месяца база не принимает");

    expect(osmod::moneyCsv(123456789) == "1234567,89", "деньги в CSV без пробелов");
    expect(osmod::csvField("Мама; мыла") == "\"Мама; мыла\"", "точка с запятой экранируется");
    expect(osmod::csvField("Он сказал \"да\"") == "\"Он сказал \"\"да\"\"\"", "кавычки удваиваются");

    store.reset();
    std::filesystem::remove("data/test-vedomost.csv");
    std::filesystem::remove("data/test-period.csv");
}

void testLowValue() {
    auto store = openTestStore();
    osmod::Service svc(*store);
    int64_t personId = store->addPerson(osmod::Person{0, "Кладов Кладов Кладович", "кладовщик"});

    osmod::Asset drill = makeAsset(personId, "МЦ-1", osmod::parseRubToKop("18990"), 0, 36, osmod::Method::Linear,
                                   {2025, 2, 2});
    svc.acceptAsset(drill, "test");

    auto lines = svc.closeMonth({2025, 3}, {}, "test");
    bool skipped = false;
    for (const osmod::CloseLine& line : lines) {
        if (line.inv == "МЦ-1") {
            skipped = line.skipped && line.amountKop == 0;
        }
    }
    expect(skipped, "дрель дешевле лимита, проводки нет");
    auto saved = store->assetByInv("МЦ-1");
    expect(saved && saved->kind == osmod::AssetKind::LowValue, "вид переписан в малоценку");
    expect(saved && saved->lifeMonths == 0, "срок малоценки обнулён");

    store.reset();
}

void testRejectionsAndRetire() {
    auto store = openTestStore();
    osmod::Service svc(*store);
    int64_t personId = store->addPerson(osmod::Person{0, "Проверкин Проверка Проверкович", "механик"});

    osmod::Asset noPerson = makeAsset(999, "ОС-8000", 20000000, 0, 12, osmod::Method::Linear);
    expectThrows([&] { svc.acceptAsset(noPerson, "test"); }, "без МОЛ карточка не принимается");

    osmod::Asset badLiq = makeAsset(personId, "ОС-8001", 20000000, 20000000, 12, osmod::Method::Linear);
    expectThrows([&] { svc.acceptAsset(badLiq, "test"); }, "ликвидационная не может равняться стоимости");

    osmod::Asset badMethod = makeAsset(personId, "ОС-8002", 20000000, 0, 12, static_cast<osmod::Method>(9));
    expectThrows([&] { svc.acceptAsset(badMethod, "test"); }, "девятый способ не проходит");

    osmod::Asset good = makeAsset(personId, "ОС-8003", 20000000, 0, 12, osmod::Method::Linear);
    int64_t id = svc.acceptAsset(good, "test");
    expectThrows([&] { svc.acceptAsset(good, "test"); }, "повторный инвентарный номер не проходит");

    svc.closeMonth({2024, 2}, {}, "test");
    expectThrows([&] { svc.retire(id, {2023, 12, 1}, "рано", "test"); },
                 "дата списания раньше принятия не проходит");
    expectThrows([&] { svc.retire(id, {2024, 3, 5}, "сломался", "test"); },
                 "незакрытый месяц списания не пропускается");
    svc.closeMonth({2024, 3}, {}, "test");
    svc.retire(id, {2024, 3, 5}, "сломался", "test");
    auto retired = store->asset(id);
    expect(retired && retired->status == osmod::AssetStatus::Retired, "объект списан");
    expectThrows([&] { svc.retire(id, {2024, 4, 1}, "ещё раз", "test"); }, "повторное списание отклоняется");

    auto lines = svc.closeMonth({2024, 4}, {}, "test");
    bool skipped = false;
    for (const osmod::CloseLine& line : lines) {
        if (line.inv == "ОС-8003") {
            skipped = line.skipped && line.note.find("списан") != std::string::npos;
        }
    }
    expect(skipped, "списанный объект пропускается при закрытии");
    expect(!svc.sheet().empty() && svc.sheet().front().status == "списано", "в ведомости стоит «списано»");

    // Месяц считается целиком до записи: ошибка на втором объекте не оставляет первый закрытым.
    osmod::Asset pendingLinear = makeAsset(personId, "ОС-8004", 20000000, 0, 12, osmod::Method::Linear, {2025, 1, 10});
    osmod::Asset pendingOutput = makeAsset(personId, "ОС-8005", 20000000, 0, 12, osmod::Method::Output, {2025, 1, 10});
    pendingOutput.planUnits = 100;
    int64_t pendingId = svc.acceptAsset(pendingLinear, "test");
    svc.acceptAsset(pendingOutput, "test");
    expectThrows([&] { svc.closeMonth({2025, 2}, {}, "test"); }, "месяц без выработки не закрывается");
    expect(store->charges(pendingId).empty(), "после ошибки месяц не записан наполовину");

    store.reset();
}

}  // namespace

int main() {
    std::filesystem::create_directories("data");
    testMoney();
    testDates();
    testLinear();
    testDeclining();
    testOutput();
    testSumOfYears();
    testRejectsBadInput();
    testDemoTotals();
    testLowValue();
    testRejectionsAndRetire();
    if (fails != 0) {
        std::cerr << fails << " проверок не сошлось\n";
        return 1;
    }
    std::cout << "все проверки сошлись\n";
    return 0;
}
