#include "ui/menu.h"

#include "ui/input.h"
#include "ui/reports.h"

#include "osmod/money.h"
#include "osmod/report.h"
#include "osmod/service.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace osmod {
namespace ui {
namespace {

constexpr const char* kDriver = "Driver={ODBC Driver 17 for SQL Server};";

std::string defaultConnection(const std::string& database) {
    return std::string(kDriver) + "Server=localhost;Database=" + database + ";Trusted_Connection=yes;";
}

std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? std::string(value) : fallback;
}

bool hasConnectionBreak(const std::string& text) {
    return text.find(';') != std::string::npos || text.find('{') != std::string::npos ||
           text.find('}') != std::string::npos;
}

std::string askConnection() {
    std::string preset = envOr("OSMOD_CONNECTION", "");
    if (!preset.empty()) {
        std::cout << "Подключение из OSMOD_CONNECTION.\n";
        return preset;
    }

    std::cout << "Строка подключения целиком (Enter = собрать по вопросам):\n";
    std::string spec = askText("> ", true);
    if (!spec.empty()) {
        if (spec.rfind("Driver=", 0) != 0) {
            throw std::runtime_error("строка подключения должна начинаться с Driver=");
        }
        return spec;
    }

    std::string server = askText("Сервер (Enter = localhost): ", true);
    std::string database = askText("База (Enter = AssetOs): ", true);
    std::string login = askText("Логин SQL (Enter = вход Windows): ", true);
    if (server.empty()) {
        server = "localhost";
    }
    if (database.empty()) {
        database = "AssetOs";
    }
    if (hasConnectionBreak(server) || hasConnectionBreak(database)) {
        throw std::runtime_error("в имени сервера или базы есть ; { }, так строку не собрать");
    }

    std::string connection = std::string(kDriver) + "Server=" + server + ";Database=" + database + ";";
    if (login.empty()) {
        connection += "Trusted_Connection=yes;";
    } else {
        if (hasConnectionBreak(login)) {
            throw std::runtime_error("в логине есть ; { }, так строку не собрать");
        }
        std::string password = askText("Пароль: ");
        if (hasConnectionBreak(password)) {
            throw std::runtime_error("в пароле есть ; { }, так строку не собрать");
        }
        connection += "UID=" + login + ";PWD=" + password + ";";
    }
    return connection;
}

void ensureParent(const std::string& path) {
    std::filesystem::path file(path);
    if (file.has_parent_path()) {
        std::filesystem::create_directories(file.parent_path());
    }
}

Period askPeriod() {
    int year = askInt("Год", 1990, 2100);
    int month = askInt("Месяц", 1, 12);
    return Period{year, month};
}

std::optional<Asset> findAsset(Store& store, const std::string& inv) {
    auto found = store.assetByInv(inv);
    if (!found) {
        std::cout << "Карточки " << inv << " нет.\n";
    }
    return found;
}

void peopleFlow(Store& store, bool canWrite) {
    printPeople(std::cout, store);
    if (!canWrite) {
        return;
    }
    if (!askYesNo("Добавить МОЛ?")) {
        return;
    }
    Person p;
    p.name = askText("ФИО: ");
    p.post = askText("Должность: ");
    std::cout << "Записан id " << store.addPerson(p) << "\n";
}

void acceptFlow(Store& store, Service& svc, const std::string& user) {
    if (store.people().empty()) {
        std::cout << "В справочнике нет МОЛ, сначала заведите хотя бы одного.\n";
        return;
    }
    printPeople(std::cout, store);
    Asset a;
    a.inv = askText("Инвентарный номер, например ОС-0100: ");
    a.title = askText("Наименование: ");
    a.personId = askInt64("id МОЛ: ", 1, 1000000000);
    a.accepted = askDate("Дата принятия");
    a.costKop = askMoneyKop("Первоначальная стоимость, руб.: ", false);
    int64_t limit = svc.valueLimitKop();
    if (a.costKop < limit) {
        std::cout << "Стоимость ниже лимита " << formatKop(limit)
                  << " руб.: карточка станет малоценкой, амортизацию не начисляю.\n";
    } else {
        a.liqKop = askMoneyKop("Ликвидационная стоимость, руб.: ", true);
        a.lifeMonths = askInt("Срок полезного использования, месяцев", 1, 600);
        a.method = askMethod();
        if (a.method == Method::Declining) {
            std::cout << "Коэффициент задаётся целым ×100: 200 значит 2,00.\n";
            a.declineX100 = askInt("Коэффициент ×100", 100, 300);
        }
        if (a.method == Method::Output) {
            a.planUnits = askInt64("План выработки, целые единицы: ", 1, 1000000000);
        }
    }
    a.place = askText("Место хранения: ", true);
    a.note = askText("Примечание: ", true);
    int64_t id = svc.acceptAsset(a, user);
    std::cout << "\nКарточка заведена, id " << id << ".\n";
    std::cout << svc.cardText(id);
}

void closeFlow(Store& store, Service& svc, const std::string& user) {
    Period p = askPeriod();
    std::map<int64_t, int64_t> units;
    for (const Asset& a : store.assets()) {
        if (a.kind != AssetKind::Fixed || a.status != AssetStatus::Active || a.method != Method::Output) {
            continue;
        }
        int64_t used = 0;
        for (const Charge& c : store.charges(a.id)) {
            used += c.units;
        }
        if (used == 0 && p < nextMonth(periodOf(a.accepted))) {
            continue;  // начисление по этому объекту ещё не началось, выработку не спрашиваю
        }
        int64_t room = a.planUnits - used;
        if (room <= 0) {
            continue;  // план выработки уже закрыт
        }
        std::cout << a.inv << " " << a.title << ": план " << a.planUnits << ", уже " << used << ", осталось "
                  << room << "\n";
        units[a.id] = askInt64("Выработка за " + periodText(p) + ", ед.: ", 0, room);
    }
    printCloseResult(std::cout, svc.closeMonth(p, units, user));
}

void cardFlow(Store& store, Service& svc) {
    std::string inv = askText("Инв. номер: ");
    auto found = findAsset(store, inv);
    if (found) {
        printCard(std::cout, svc, found->id);
    }
}

void periodFlow(Service& svc) {
    PeriodReport report = svc.periodReport(askPeriod());
    printPeriodReport(std::cout, report);
}

void yearFlow(Service& svc) {
    int year = askInt("Год", 1990, 2100);
    printYearTotals(std::cout, svc.yearTotals(year));
}

void forecastFlow(Store& store, Service& svc) {
    std::string inv = askText("Инв. номер: ");
    auto found = findAsset(store, inv);
    if (!found) {
        return;
    }
    int months = askInt("Горизонт прогноза, месяцев", 1, 120);
    int64_t units = 0;
    if (found->kind == AssetKind::Fixed && found->method == Method::Output) {
        units = askInt64("Средняя выработка в месяц (0 = посчитать по плану): ", 0, 1000000000);
    }
    printForecast(std::cout, svc.forecast(found->id, months, units));
}

void retireFlow(Store& store, Service& svc, const std::string& user) {
    std::string inv = askText("Инв. номер: ");
    auto found = findAsset(store, inv);
    if (!found) {
        return;
    }
    DateYmd date = askDate("Дата списания");
    std::string reason = askText("Причина списания: ");
    std::cout << "Если месяц списания ещё не закрыт, сначала придётся закрыть его.\n";
    if (!askYesNo("Списываем " + found->inv + "?")) {
        return;
    }
    svc.retire(found->id, date, reason, user);
    std::cout << "Списано: " << found->inv << "\n";
}

void reopenFlow(Service& svc, const std::string& user) {
    auto last = svc.lastClosedPeriod();
    if (!last) {
        std::cout << "Закрытых месяцев ещё нет.\n";
        return;
    }
    std::cout << "Последний закрытый месяц: " << periodText(*last) << "\n";
    if (!askYesNo("Снять проводки этого месяца?")) {
        return;
    }
    Period removed = svc.reopenLastMonth(user);
    std::cout << "Проводки за " << periodText(removed) << " сняты, месяц можно закрыть заново.\n";
}

void backupFlow(Store& store) {
    std::cout << "Путь видит служба SQL Server на своей машине, а не ваш проводник.\n";
    std::string path = askText("Куда сохранить копию (Enter = папка SQL Server по умолчанию): ", true);
    store.backupTo(path);
    std::cout << "Копия записана: " << (path.empty() ? "в папку резервных копий SQL Server" : path) << "\n";
}

void seedFlow(Service& svc, const std::string& user) {
    if (!askYesNo("Завести учебную фирму в пустую базу?")) {
        return;
    }
    svc.seedDemo(user);
    std::cout << "Учебные данные заведены.\n";
}

std::string askPath(const std::string& prompt, const std::string& fallback) {
    std::string path = askText(prompt + " (Enter = " + fallback + "): ", true);
    return path.empty() ? fallback : path;
}

void exportSheetFlow(const Service& svc) {
    std::string path = askPath("Файл CSV", "data/vedomost.csv");
    ensureParent(path);
    std::string error;
    if (exportSheetCsv(svc, path, error)) {
        std::cout << "Ведомость выгружена: " << path << "\n";
    } else {
        std::cout << "Не вышло: " << error << "\n";
    }
}

void exportPeriodFlow(const Service& svc) {
    PeriodReport report = svc.periodReport(askPeriod());
    if (!report.closed) {
        std::cout << "Месяц ещё не закрыт, выгружать нечего.\n";
        return;
    }
    std::ostringstream name;
    name << "data/period-" << report.period.year << '-' << (report.period.month < 10 ? "0" : "")
         << report.period.month << ".csv";
    std::string path = askPath("Файл CSV", name.str());
    ensureParent(path);
    std::string error;
    if (exportPeriodCsv(report, path, error)) {
        std::cout << "Отчёт выгружен: " << path << "\n";
    } else {
        std::cout << "Не вышло: " << error << "\n";
    }
}

void printMenu(const std::string& role) {
    std::cout << "\nМеню, роль «" << role << "»\n";
    std::cout << " 1 сводка по фирме          2 справочник МОЛ\n";
    std::cout << " 3 ведомость остатков       4 карточка объекта\n";
    std::cout << " 5 отчёт за месяц           6 итоги за год\n";
    std::cout << " 7 прогноз амортизации      8 журнал действий\n";
    std::cout << " 9 способы начисления      10 принять объект\n";
    std::cout << "11 закрыть месяц           12 списать объект\n";
    std::cout << "13 отменить закрытие       14 резервная копия\n";
    std::cout << "15 учебные данные          16 ведомость в CSV\n";
    std::cout << "17 месяц в CSV              0 выход\n";
}

}  // namespace

void printUsage() {
    std::cout << "osmod — учебный учёт основных средств и амортизации, SQL Server\n";
    std::cout << "  osmod                 меню, подключение спрашивается, по умолчанию localhost / AssetOs\n";
    std::cout << "  osmod <строка>        сразу открыть базу по строке подключения (Driver=...)\n";
    std::cout << "  osmod --demo [база]   учебный сценарий в AssetOsDemo, база предварительно очищается\n";
    std::cout << "  osmod --help          эта справка\n\n";
    std::cout << "Роли берутся из базы: os_accountant пишет, os_reader только смотрит.\n";
    std::cout << "Скрипты: sql/sqlserver/01_create_db.sql, 02_schema.sql, 03_roles_audit.sql,\n";
    std::cout << "04_seed.sql, 05_backup.sql. Переменная OSMOD_CONNECTION задаёт строку без вопросов.\n";
}

int runInteractive(const std::string& connection) {
    std::string spec = connection.empty() ? askConnection() : connection;
    if (spec.rfind("Driver=", 0) != 0) {
        throw std::runtime_error("строка подключения должна начинаться с Driver=");
    }

    auto store = openSqlServer(spec);
    std::cout << "Движок: " << store->engineName() << "\n";
    try {
        store->migrate();
    } catch (const std::exception& ex) {
        std::cout << "Схему не подготовил: " << ex.what() << "\n";
        std::cout << "Если база уже создана скриптом 02_schema.sql, для роли чтения это нормально.\n";
    }

    std::string login = store->currentLogin();
    std::string role = store->currentRole();
    std::cout << "Пользователь: " << login << "\n";
    if (role.empty()) {
        std::cout << "У этого входа нет роли в базе. Под администратором выполните sql/sqlserver/03_roles_audit.sql\n";
        std::cout << "и включите вход в os_accountant или os_reader.\n";
        return 1;
    }
    std::cout << "Роль: " << role << "\n";
    bool canWrite = role == "бухгалтер";
    Service svc(*store);

    if (store->people().empty() && store->assets().empty()) {
        if (canWrite) {
            std::cout << "База пустая: ни людей, ни карточек.\n";
            if (askYesNo("Завести учебную фирму?")) {
                svc.seedDemo(login);
                std::cout << "Учебные данные заведены.\n";
            }
        } else {
            std::cout << "База пустая, а роль только смотрит: учебные данные заводит бухгалтер.\n";
        }
    }

    while (true) {
        printMenu(role);
        int command = askInt("Команда", 0, 17);
        if (command == 0) {
            break;
        }
        try {
            bool needsWrite = command == 10 || command == 11 || command == 12 || command == 13 || command == 14 ||
                              command == 15;
            if (needsWrite && !canWrite) {
                std::cout << "Роль «" << role << "» только смотрит, запись закрыта.\n";
                continue;
            }
            switch (command) {
                case 1:
                    printSummary(std::cout, svc, store->engineName(), store->setting("company", ""), role);
                    break;
                case 2:
                    peopleFlow(*store, canWrite);
                    break;
                case 3:
                    printSheet(std::cout, svc);
                    break;
                case 4:
                    cardFlow(*store, svc);
                    break;
                case 5:
                    periodFlow(svc);
                    break;
                case 6:
                    yearFlow(svc);
                    break;
                case 7:
                    forecastFlow(*store, svc);
                    break;
                case 8: {
                    int limit = askInt("Сколько последних записей", 1, 200);
                    printJournal(std::cout, svc, limit);
                    break;
                }
                case 9:
                    printMethods(std::cout);
                    break;
                case 10:
                    acceptFlow(*store, svc, login);
                    break;
                case 11:
                    closeFlow(*store, svc, login);
                    break;
                case 12:
                    retireFlow(*store, svc, login);
                    break;
                case 13:
                    reopenFlow(svc, login);
                    break;
                case 14:
                    backupFlow(*store);
                    break;
                case 15:
                    seedFlow(svc, login);
                    break;
                case 16:
                    exportSheetFlow(svc);
                    break;
                case 17:
                    exportPeriodFlow(svc);
                    break;
                default:
                    break;
            }
        } catch (const std::exception& ex) {
            std::cout << "Не вышло: " << ex.what() << "\n";
        }
    }
    return 0;
}

int runDemo(const std::string& connection) {
    std::string spec;
    if (!connection.empty() && connection.rfind("Driver=", 0) == 0) {
        spec = connection;
    } else {
        std::string database = connection.empty() ? envOr("OSMOD_DEMO_DATABASE", "AssetOsDemo") : connection;
        spec = defaultConnection(database);
    }

    auto store = openSqlServer(spec);
    store->migrate();
    store->resetAll();
    Service svc(*store);
    svc.seedDemo("demo");

    std::string role = store->currentRole();
    if (role.empty()) {
        role = "бухгалтер";
    }

    std::cout << "Движок: " << store->engineName() << "\n";
    std::cout << "Пользователь: " << store->currentLogin() << ", роль: " << role << "\n";
    printSummary(std::cout, svc, store->engineName(), store->setting("company", ""), role);
    std::cout << "Лимит признания ОС: " << formatKop(svc.valueLimitKop()) << " руб.\n";

    std::map<Period, int64_t> cradleHours{
        {{2024, 7}, 160},  {{2024, 8}, 210}, {{2024, 9}, 40},   {{2024, 10}, 180}, {{2024, 11}, 90},
        {{2024, 12}, 20},  {{2025, 1}, 0},   {{2025, 2}, 150},  {{2025, 3}, 175},  {{2025, 4}, 200},
    };
    auto cradle = store->assetByInv("ОС-0003");

    for (int i = 0; i < 13; ++i) {
        Period p = nextMonth({2024, 3});
        int cursor = i;
        while (cursor-- > 0) {
            p = nextMonth(p);
        }
        std::map<int64_t, int64_t> units;
        auto hours = cradleHours.find(p);
        if (hours != cradleHours.end() && cradle) {
            units[cradle->id] = hours->second;
        }
        std::cout << "\n=== закрытие " << periodText(p) << " ===\n";
        printCloseResult(std::cout, svc.closeMonth(p, units, "demo"), false);
    }

    std::cout << "\n=== повторное закрытие 04.2024 ===\n";
    try {
        svc.closeMonth({2024, 4}, {}, "demo");
        std::cout << "ошибка: повтор прошёл, так быть не должно\n";
    } catch (const std::exception& ex) {
        std::cout << ex.what() << "\n";
    }

    auto drill = store->assetByInv("МЦ-0015");
    if (drill) {
        svc.retire(drill->id, {2025, 4, 28}, "разбит на объекте, восстановлению не подлежит", "demo");
        std::cout << "\nСписан " << drill->inv << "\n";
    }

    if (auto comp = store->assetByInv("ОС-0002")) {
        printCard(std::cout, svc, comp->id);
    }
    if (auto gaz = store->assetByInv("ОС-0001")) {
        printCard(std::cout, svc, gaz->id);
    }
    if (cradle) {
        printCard(std::cout, svc, cradle->id);
    }
    printSheet(std::cout, svc);
    printPeriodReport(std::cout, svc.periodReport({2025, 4}));
    printYearTotals(std::cout, svc.yearTotals(2024));
    printYearTotals(std::cout, svc.yearTotals(2025));
    if (auto comp = store->assetByInv("ОС-0002")) {
        printForecast(std::cout, svc.forecast(comp->id, 6));
    }
    if (cradle) {
        printForecast(std::cout, svc.forecast(cradle->id, 6));
    }
    printJournal(std::cout, svc, 12);

    std::string error;
    if (exportSheetCsv(svc, "data/vedomost.csv", error)) {
        std::cout << "\nCSV ведомости: data/vedomost.csv\n";
    } else {
        std::cout << "\nCSV ведомости не вышел: " << error << "\n";
    }
    if (exportPeriodCsv(svc.periodReport({2025, 4}), "data/period-2025-04.csv", error)) {
        std::cout << "CSV отчёта: data/period-2025-04.csv\n";
    } else {
        std::cout << "CSV отчёта не вышел: " << error << "\n";
    }

    int64_t beforeReopen = svc.summary().accumulatedKop;
    Period reopened = svc.reopenLastMonth("demo");
    std::cout << "\n=== отмена закрытия " << periodText(reopened) << " ===\n";
    std::cout << "Начислено после отмены: " << formatKop(svc.summary().accumulatedKop) << " руб.\n";
    std::map<int64_t, int64_t> reopenUnits;
    auto hours = cradleHours.find(reopened);
    if (hours != cradleHours.end() && cradle) {
        reopenUnits[cradle->id] = hours->second;
    }
    printCloseResult(std::cout, svc.closeMonth(reopened, reopenUnits, "demo"), false);
    int64_t afterReopen = svc.summary().accumulatedKop;
    std::cout << "Начислено после повторного закрытия: " << formatKop(afterReopen) << " руб.";
    std::cout << (afterReopen == beforeReopen ? " (сошлось с прежним итогом)\n" : " (не сошлось!)\n");

    printSummary(std::cout, svc, store->engineName(), store->setting("company", ""), role);

    std::string backup = envOr("OSMOD_BACKUP", "");
    try {
        store->backupTo(backup);
        std::cout << "\nРезервная копия: "
                  << (backup.empty() ? "папка резервных копий SQL Server по умолчанию" : backup) << "\n";
    } catch (const std::exception& ex) {
        std::cout << "\nКопию не сделал: " << ex.what() << "\n";
        std::cout << "Путь видит служба SQL Server; поправьте его в 05_backup.sql или в OSMOD_BACKUP.\n";
    }
    return 0;
}

}  // namespace ui
}  // namespace osmod
