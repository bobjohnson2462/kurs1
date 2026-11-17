#include "osmod/service.h"

#include "osmod/depreciation.h"
#include "osmod/money.h"

#include <sstream>
#include <stdexcept>

namespace osmod {
namespace {

std::string isoDate(DateYmd d) {
    std::ostringstream os;
    os << d.year << '-' << (d.month < 10 ? "0" : "") << d.month << '-' << (d.day < 10 ? "0" : "") << d.day;
    return os.str();
}

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    return s.substr(i);
}

// Только для ASCII-значений таблиц карточки: даты, суммы и числа.
std::string padRight(const std::string& text, size_t width) {
    if (text.size() >= width) {
        return text;
    }
    return text + std::string(width - text.size(), ' ');
}

std::string padLeft(const std::string& text, size_t width) {
    if (text.size() >= width) {
        return text;
    }
    return std::string(width - text.size(), ' ') + text;
}

}  // namespace

Service::Service(Store& store) : store_(store) {}

int64_t Service::valueLimitKop() const {
    try {
        int64_t value = std::stoll(store_.setting("limit_kop", "10000000"));
        if (value > 0) {
            return value;
        }
    } catch (const std::exception&) {
        // испорченную настройку не даю уронить приём объектов
    }
    return 10000000;
}

void Service::ensurePolicy() {
    if (store_.setting("limit_kop", "").empty()) {
        store_.setSetting("limit_kop", "10000000");
    }
    if (store_.setting("company", "").empty()) {
        store_.setSetting("company", "ООО «Кровля-Сервис»");
    }
    if (store_.setting("start_rule", "").empty()) {
        store_.setSetting("start_rule", "со следующего месяца после принятия");
    }
}

void Service::seedDemo(const std::string& user) {
    if (!store_.people().empty() || !store_.assets().empty()) {
        throw std::runtime_error("в базе уже есть люди или карточки, учебную фирму поверх не кладу");
    }
    ensurePolicy();
    Person director{0, "Соколов Игорь Петрович", "директор"};
    Person master{0, "Фомин Артём Сергеевич", "прораб"};
    Person storekeeper{0, "Нуриева Дина Рашидовна", "кладовщик"};
    int64_t idDirector = store_.addPerson(director);
    int64_t idMaster = store_.addPerson(master);
    int64_t idStore = store_.addPerson(storekeeper);

    Asset gaz;
    gaz.inv = "ОС-0001";
    gaz.title = "Газель Next, гос. номер А323ММ 750";
    gaz.personId = idDirector;
    gaz.method = Method::Declining;
    gaz.accepted = DateYmd{2024, 3, 18};
    gaz.costKop = parseRubToKop("1640000");
    gaz.liqKop = parseRubToKop("240000");
    gaz.lifeMonths = 60;
    gaz.declineX100 = 200;
    gaz.place = "стоянка у ангара, Подольск";
    gaz.note = "коэффициент 2 закреплён в учётной политике на 2024 год";
    acceptAsset(gaz, user);

    Asset comp;
    comp.inv = "ОС-0002";
    comp.title = "Компрессор Aurora Storm 90";
    comp.personId = idMaster;
    comp.method = Method::Linear;
    comp.accepted = DateYmd{2025, 1, 20};
    comp.costKop = parseRubToKop("186400");
    comp.liqKop = parseRubToKop("6400");
    comp.lifeMonths = 24;
    comp.place = "бокс 2";
    comp.note = "принимали по накладной 14, без отдельной доставки";
    acceptAsset(comp, user);

    Asset cradle;
    cradle.inv = "ОС-0003";
    cradle.title = "Люлька фасадная ZLP-630";
    cradle.personId = idMaster;
    cradle.method = Method::Output;
    cradle.accepted = DateYmd{2024, 6, 2};
    cradle.costKop = parseRubToKop("510000");
    cradle.liqKop = parseRubToKop("30000");
    cradle.lifeMonths = 48;
    cradle.planUnits = 12000;
    cradle.place = "объект, когда не в боксе";
    cradle.note = "выработка в моточасах, план 12000";
    acceptAsset(cradle, user);

    Asset laptop;
    laptop.inv = "МЦ-0014";
    laptop.title = "Ноутбук бухгалтера Honor";
    laptop.personId = idStore;
    laptop.method = Method::Linear;
    laptop.accepted = DateYmd{2025, 9, 3};
    laptop.costKop = parseRubToKop("67000");
    laptop.liqKop = 0;
    laptop.lifeMonths = 0;
    laptop.place = "офис, стол у окна";
    laptop.note = "ниже лимита 100000, на балансе как малоценка, амортизацию не крутим";
    acceptAsset(laptop, user);

    Asset drill;
    drill.inv = "МЦ-0015";
    drill.title = "Перфоратор DeWalt";
    drill.personId = idMaster;
    drill.accepted = DateYmd{2025, 4, 11};
    drill.costKop = parseRubToKop("18990");
    drill.place = "кладовая, стеллаж 3";
    drill.note = "инструмент, за балансом не висит, просто чтобы не потерять";
    acceptAsset(drill, user);

    store_.audit(user, "учебные данные", "заведена ООО «Кровля-Сервис»");
}

int64_t Service::acceptAsset(const Asset& draft, const std::string& user) {
    ensurePolicy();
    Asset a = draft;
    a.inv = trim(a.inv);
    a.title = trim(a.title);
    a.place = trim(a.place);
    if (a.inv.empty() || a.inv.size() > 20) {
        throw std::runtime_error("инвентарный номер пустой или длиннее 20 знаков");
    }
    if (a.title.empty() || a.title.size() > 200) {
        throw std::runtime_error("название карточки пустое или слишком длинное");
    }
    if (!store_.person(a.personId)) {
        throw std::runtime_error("такого МОЛ в справочнике нет");
    }
    if (!validDate(a.accepted)) {
        throw std::runtime_error("дата принятия не календарная");
    }
    if (a.costKop <= 0) {
        throw std::runtime_error("первоначальная стоимость должна быть больше нуля");
    }
    if (a.liqKop < 0) {
        throw std::runtime_error("ликвидационная не бывает отрицательной");
    }
    if (store_.assetByInv(a.inv)) {
        throw std::runtime_error("инвентарный номер " + a.inv + " уже занят");
    }

    int64_t limit = valueLimitKop();
    if (a.costKop < limit) {
        a.kind = AssetKind::LowValue;
        a.liqKop = 0;
        a.lifeMonths = 0;
        a.planUnits = 0;
        a.declineX100 = 0;
        a.method = Method::Linear;
    } else {
        a.kind = AssetKind::Fixed;
        if (a.liqKop >= a.costKop) {
            throw std::runtime_error("ликвидационная должна быть меньше первоначальной");
        }
        if (a.lifeMonths < 1 || a.lifeMonths > 600) {
            throw std::runtime_error("срок полезного использования задайте от 1 до 600 месяцев");
        }
        if (a.method == Method::Declining && (a.declineX100 < 100 || a.declineX100 > 300)) {
            throw std::runtime_error("коэффициент уменьшаемого остатка держу в пределах от 1,00 до 3,00");
        }
        if (a.method == Method::Output && a.planUnits <= 0) {
            throw std::runtime_error("для выработки нужен план больше нуля");
        }
        int methodCode = static_cast<int>(a.method);
        if (methodCode < 1 || methodCode > kMethodCount) {
            throw std::runtime_error("неизвестный способ амортизации");
        }
        if (a.method != Method::Output) {
            a.planUnits = 0;
        }
        if (a.method != Method::Declining) {
            a.declineX100 = 0;
        }
    }
    a.status = AssetStatus::Active;
    int64_t id = store_.addAsset(a);
    store_.audit(user, "принятие", a.inv + " " + a.title + " на " + formatKop(a.costKop));
    return id;
}

std::vector<CloseLine> Service::closeMonth(Period p, const std::map<int64_t, int64_t>& unitsByAsset,
                                            const std::string& user) {
    if (p.month < 1 || p.month > 12 || p.year < 1990) {
        throw std::runtime_error("месяц закрытия задан криво");
    }
    // Сначала считаю весь месяц в памяти и только потом пишу проводки.
    // Иначе ошибка на пятом объекте оставила бы месяц наполовину закрытым.
    std::vector<CloseLine> lines;
    std::vector<Charge> pending;
    int64_t total = 0;
    for (const Asset& a : store_.assets()) {
        CloseLine line;
        line.inv = a.inv;
        line.title = a.title;
        if (a.status != AssetStatus::Active || a.kind != AssetKind::Fixed) {
            line.skipped = true;
            line.note = a.kind == AssetKind::LowValue ? "малоценка, амортизации нет" : "объект списан";
            line.bookKop = a.costKop;
            lines.push_back(line);
            continue;
        }
        Period start = nextMonth(periodOf(a.accepted));
        if (p < start) {
            line.skipped = true;
            line.note = "срок начисления ещё не начался, первое окно " + periodText(start);
            line.bookKop = a.costKop;
            lines.push_back(line);
            continue;
        }
        if (store_.hasCharge(a.id, p)) {
            throw std::runtime_error("по " + a.inv + " месяц " + periodText(p) + " уже закрыт");
        }
        auto history = store_.charges(a.id);
        int64_t acc = 0;
        int64_t accUnits = 0;
        for (const Charge& c : history) {
            acc += c.amountKop;
            accUnits += c.units;
        }
        int64_t left = (a.costKop - a.liqKop) - acc;
        if (left <= 0) {
            line.skipped = true;
            line.note = "ликвидационная уже достигнута, дальше не кручу";
            line.bookKop = a.costKop - acc;
            lines.push_back(line);
            continue;
        }
        if (history.empty()) {
            if (!(p == start)) {
                throw std::runtime_error("по " + a.inv + " первое начисление должно быть за " + periodText(start) +
                                          ", дырку в графике не оставляю");
            }
        } else {
            Period expect = nextMonth(history.back().period);
            if (!(p == expect)) {
                throw std::runtime_error("по " + a.inv + " жду месяц " + periodText(expect) + ", а пришёл " +
                                          periodText(p));
            }
        }

        DepreciationInput in;
        in.costKop = a.costKop;
        in.liqKop = a.liqKop;
        in.lifeMonths = a.lifeMonths;
        in.declineX100 = a.declineX100;
        in.planUnits = a.planUnits;
        in.accumulatedKop = acc;
        in.accumulatedUnits = accUnits;
        in.monthsAlready = static_cast<int>(history.size());
        in.method = a.method;
        in.forceRemainder = in.monthsAlready >= a.lifeMonths - 1;
        if (a.method == Method::Output && !in.forceRemainder) {
            auto found = unitsByAsset.find(a.id);
            if (found == unitsByAsset.end()) {
                throw std::runtime_error("для " + a.inv + " не передана выработка за " + periodText(p));
            }
            in.unitsThisPeriod = found->second;
        } else if (a.method == Method::Output) {
            auto found = unitsByAsset.find(a.id);
            in.unitsThisPeriod = found == unitsByAsset.end() ? 0 : found->second;
        }

        DepreciationOutput got = chargeDepreciation(in);
        Charge row;
        row.assetId = a.id;
        row.period = p;
        row.amountKop = got.amountKop;
        row.bookKop = got.bookKop;
        row.units = in.unitsThisPeriod;
        pending.push_back(row);
        line.amountKop = got.amountKop;
        line.bookKop = got.bookKop;
        line.note = got.note;
        total += got.amountKop;
        lines.push_back(line);
    }
    for (const Charge& row : pending) {
        store_.addCharge(row);
    }
    store_.audit(user, "закрытие месяца", periodText(p) + " начислено " + formatKop(total));
    return lines;
}

void Service::retire(int64_t assetId, DateYmd date, const std::string& reason, const std::string& user) {
    auto found = store_.asset(assetId);
    if (!found) {
        throw std::runtime_error("карточки с таким id нет");
    }
    if (found->status != AssetStatus::Active) {
        throw std::runtime_error("объект уже списан");
    }
    if (!validDate(date)) {
        throw std::runtime_error("дата списания не календарная");
    }
    if (trim(reason).empty()) {
        throw std::runtime_error("напишите, почему списываете");
    }
    if (periodOf(date) < periodOf(found->accepted)) {
        throw std::runtime_error("дата списания раньше месяца принятия");
    }
    if (found->kind == AssetKind::Fixed) {
        Period start = nextMonth(periodOf(found->accepted));
        Period retireMonth = periodOf(date);
        if (!(retireMonth < start) && !store_.hasCharge(assetId, retireMonth)) {
            throw std::runtime_error("сначала закройте амортизацию за " + periodText(retireMonth) +
                                      ", иначе месяц выбытия выпадет");
        }
    }
    store_.retireAsset(assetId, isoDate(date), trim(reason));
    store_.audit(user, "списание", found->inv + " " + isoDate(date) + " " + trim(reason));
}

std::vector<SheetLine> Service::sheet() const {
    std::vector<SheetLine> rows;
    for (const Asset& a : store_.assets()) {
        SheetLine line;
        line.inv = a.inv;
        line.title = a.title;
        line.method = a.kind == AssetKind::Fixed ? strategyFor(a.method).name() : "не амортизируется";
        line.costKop = a.costKop;
        int64_t acc = 0;
        auto history = store_.charges(a.id);
        for (const Charge& c : history) {
            acc += c.amountKop;
        }
        line.accumulatedKop = acc;
        line.bookKop = a.costKop - acc;
        auto who = store_.person(a.personId);
        line.person = who ? who->name : "?";
        line.status = statusName(a.status);
        rows.push_back(line);
    }
    return rows;
}

std::string Service::cardText(int64_t assetId) const {
    auto found = store_.asset(assetId);
    if (!found) {
        throw std::runtime_error("карточки нет");
    }
    const Asset& a = *found;
    auto who = store_.person(a.personId);
    std::ostringstream os;
    os << "Инвентарная карточка (учебный аналог ОС-6)\n";
    os << "Фирма: " << store_.setting("company", "") << "\n";
    os << "Номер: " << a.inv << "\n";
    os << "Наименование: " << a.title << "\n";
    os << "Вид: " << kindName(a.kind) << "\n";
    os << "Состояние: " << statusName(a.status) << "\n";
    os << "МОЛ: " << (who ? who->name + ", " + who->post : "?") << "\n";
    os << "Принят: " << dateText(a.accepted) << "\n";
    os << "Место: " << a.place << "\n";
    os << "Первоначальная: " << formatKop(a.costKop) << " руб.\n";
    if (a.kind == AssetKind::Fixed) {
        os << "Ликвидационная: " << formatKop(a.liqKop) << " руб.\n";
        os << "Срок: " << a.lifeMonths << " мес.\n";
        os << "Способ: " << strategyFor(a.method).name() << "\n";
        if (a.method == Method::Declining) {
            int whole = a.declineX100 / 100;
            int frac = a.declineX100 % 100;
            os << "Коэффициент: " << whole << "," << (frac < 10 ? "0" : "") << frac << "\n";
        }
        if (a.method == Method::Output) {
            os << "План выработки: " << a.planUnits << "\n";
        }
        os << "Начисление с: " << periodText(nextMonth(periodOf(a.accepted))) << "\n";
    }
    os << "Примечание: " << a.note << "\n";
    if (a.status == AssetStatus::Retired) {
        os << "Списан: " << a.retiredOn << " (" << a.retireReason << ")\n";
    }
    os << "Месяц          Начислено     Балансовая   Выработка\n";
    int64_t acc = 0;
    for (const Charge& c : store_.charges(a.id)) {
        acc += c.amountKop;
        os << padRight(periodText(c.period), 10) << padLeft(formatKop(c.amountKop), 14)
           << padLeft(formatKop(c.bookKop), 15) << padLeft(std::to_string(c.units), 12) << "\n";
    }
    os << "Итого накоплено: " << formatKop(acc) << " руб.\n";
    os << "Балансовая сейчас: " << formatKop(a.costKop - acc) << " руб.\n";
    return os.str();
}

std::optional<Period> Service::lastClosedPeriod() const {
    std::optional<Period> last;
    for (const Charge& c : store_.chargesAll()) {
        if (!last || *last < c.period) {
            last = c.period;
        }
    }
    return last;
}

Summary Service::summary() const {
    Summary s;
    for (const Asset& a : store_.assets()) {
        ++s.totalAssets;
        s.costKop += a.costKop;
        if (a.kind == AssetKind::LowValue) {
            ++s.lowValueAssets;
            s.lowValueKop += a.costKop;
        } else {
            ++s.fixedAssets;
            s.fixedCostKop += a.costKop;
        }
        if (a.status == AssetStatus::Active) {
            ++s.activeAssets;
        } else {
            ++s.retiredAssets;
        }
    }
    for (const Charge& c : store_.chargesAll()) {
        s.accumulatedKop += c.amountKop;
        if (!s.lastClosed || *s.lastClosed < c.period) {
            s.lastClosed = c.period;
        }
    }
    s.bookKop = s.costKop - s.accumulatedKop;
    return s;
}

PeriodReport Service::periodReport(Period p) const {
    PeriodReport report;
    report.period = p;
    for (const Charge& c : store_.chargesForPeriod(p)) {
        PeriodLine line;
        auto owner = store_.asset(c.assetId);
        line.inv = owner ? owner->inv : "?";
        line.title = owner ? owner->title : "карточка не найдена";
        auto who = owner ? store_.person(owner->personId) : std::optional<Person>{};
        line.person = who ? who->name : "?";
        line.amountKop = c.amountKop;
        line.bookKop = c.bookKop;
        line.units = c.units;
        report.totalKop += c.amountKop;
        report.totalUnits += c.units;
        report.lines.push_back(line);
    }
    report.closed = !report.lines.empty();
    return report;
}

YearTotals Service::yearTotals(int year) const {
    YearTotals totals;
    totals.year = year;
    bool seen[12] = {};
    for (const Charge& c : store_.chargesAll()) {
        if (c.period.year != year) {
            continue;
        }
        totals.byMonth[c.period.month - 1] += c.amountKop;
        totals.totalKop += c.amountKop;
        seen[c.period.month - 1] = true;
    }
    for (bool month : seen) {
        if (month) {
            ++totals.monthsClosed;
        }
    }
    return totals;
}

Forecast Service::forecast(int64_t assetId, int months, int64_t unitsPerMonth) const {
    auto found = store_.asset(assetId);
    if (!found) {
        throw std::runtime_error("карточки с таким id нет");
    }
    if (months < 1 || months > 600) {
        throw std::runtime_error("горизонт прогноза задайте от 1 до 600 месяцев");
    }
    const Asset& a = *found;
    Forecast forecast;
    forecast.inv = a.inv;
    forecast.title = a.title;
    forecast.method = a.kind == AssetKind::Fixed ? strategyFor(a.method).name() : "не амортизируется";
    forecast.floorKop = a.kind == AssetKind::Fixed ? a.liqKop : a.costKop;
    forecast.bookKop = a.costKop;
    if (a.kind != AssetKind::Fixed) {
        forecast.note = "малоценка, амортизацию не кручу";
        return forecast;
    }
    if (a.status != AssetStatus::Active) {
        forecast.note = "объект списан, дальше начислять нечего";
        return forecast;
    }

    auto history = store_.charges(a.id);
    int64_t accumulated = 0;
    int64_t accumulatedUnits = 0;
    for (const Charge& c : history) {
        accumulated += c.amountKop;
        accumulatedUnits += c.units;
    }
    forecast.bookKop = a.costKop - accumulated;
    int monthsAlready = static_cast<int>(history.size());
    Period p = history.empty() ? nextMonth(periodOf(a.accepted)) : nextMonth(history.back().period);

    if (a.method == Method::Output && unitsPerMonth <= 0) {
        unitsPerMonth = a.planUnits / (a.lifeMonths > 0 ? a.lifeMonths : 1);
        if (unitsPerMonth < 1) {
            unitsPerMonth = 1;
        }
        forecast.note = "выработка принята ровной: " + std::to_string(unitsPerMonth) + " ед./мес.";
    }

    for (int i = 0; i < months; ++i) {
        int64_t left = (a.costKop - a.liqKop) - accumulated;
        if (left <= 0) {
            forecast.stoppedAtFloor = true;
            break;
        }
        DepreciationInput in;
        in.costKop = a.costKop;
        in.liqKop = a.liqKop;
        in.lifeMonths = a.lifeMonths;
        in.declineX100 = a.declineX100;
        in.planUnits = a.planUnits;
        in.accumulatedKop = accumulated;
        in.accumulatedUnits = accumulatedUnits;
        in.monthsAlready = monthsAlready;
        in.method = a.method;
        in.forceRemainder = monthsAlready >= a.lifeMonths - 1;
        if (a.method == Method::Output) {
            int64_t room = a.planUnits - accumulatedUnits;
            if (room < 0) {
                room = 0;
            }
            in.unitsThisPeriod = unitsPerMonth < room ? unitsPerMonth : room;
        }
        DepreciationOutput got = chargeDepreciation(in);
        ForecastLine line;
        line.period = p;
        line.amountKop = got.amountKop;
        line.bookKop = got.bookKop;
        line.units = in.unitsThisPeriod;
        line.note = got.note;
        forecast.lines.push_back(line);
        forecast.totalKop += got.amountKop;
        accumulated += got.amountKop;
        accumulatedUnits += in.unitsThisPeriod;
        ++monthsAlready;
        p = nextMonth(p);
        if (got.reachedFloor) {
            forecast.stoppedAtFloor = true;
            break;
        }
    }
    return forecast;
}

Period Service::reopenLastMonth(const std::string& user) {
    auto last = lastClosedPeriod();
    if (!last) {
        throw std::runtime_error("в базе ещё нет ни одного закрытого месяца");
    }
    int64_t total = 0;
    for (const Charge& c : store_.chargesForPeriod(*last)) {
        total += c.amountKop;
    }
    store_.deleteChargesFor(*last);
    store_.audit(user, "отмена закрытия", periodText(*last) + ", снято начислений на " + formatKop(total) + " руб.");
    return *last;
}

std::vector<std::string> Service::journal(int limit) const {
    return store_.auditTail(limit);
}

}  // namespace osmod
