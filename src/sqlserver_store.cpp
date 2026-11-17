#include "osmod/store.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sql.h>
#include <sqlext.h>

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace osmod {
namespace {

// ODBC на Windows разговаривает с SQL Server в UTF-16, а консоль у нас в UTF-8.
// Поэтому все строки туда-обратно гоняю через wchar_t, иначе кириллица лезет ромбиками.
std::wstring toWide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string toUtf8(const std::wstring& w) {
    if (w.empty()) {
        return {};
    }
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) {
        return {};
    }
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

std::string diag(SQLSMALLINT type, SQLHANDLE handle) {
    SQLWCHAR state[8] = {0};
    SQLWCHAR text[1024] = {0};
    SQLINTEGER native = 0;
    SQLSMALLINT len = 0;
    SQLRETURN rc = SQLGetDiagRecW(type, handle, 1, state, &native, text, sizeof(text) / sizeof(text[0]), &len);
    if (!SQL_SUCCEEDED(rc)) {
        return "ODBC без текста ошибки";
    }
    std::ostringstream os;
    os << toUtf8(std::wstring(state)) << " (" << native << ") " << toUtf8(std::wstring(text));
    return os.str();
}

class SqlServerStore : public Store {
public:
    explicit SqlServerStore(std::string connection) : connection_(std::move(connection)) {
        if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_))) {
            throw std::runtime_error("ODBC: не выделил окружение");
        }
        SQLSetEnvAttr(env_, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
        if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_DBC, env_, &dbc_))) {
            throw std::runtime_error(diag(SQL_HANDLE_ENV, env_));
        }
        SQLWCHAR out[1024];
        SQLSMALLINT outLen = 0;
        std::wstring conn = toWide(connection_);
        SQLRETURN rc = SQLDriverConnectW(dbc_, nullptr, reinterpret_cast<SQLWCHAR*>(conn.data()), SQL_NTS, out,
                                         sizeof(out) / sizeof(out[0]), &outLen, SQL_DRIVER_NOPROMPT);
        if (!SQL_SUCCEEDED(rc)) {
            throw std::runtime_error("SQL Server не пустил: " + diag(SQL_HANDLE_DBC, dbc_) +
                                      ". Проверьте драйвер ODBC, сервер и что базы созданы скриптом 01_create_db.sql.");
        }
    }

    ~SqlServerStore() override {
        if (dbc_ != SQL_NULL_HDBC) {
            SQLDisconnect(dbc_);
            SQLFreeHandle(SQL_HANDLE_DBC, dbc_);
        }
        if (env_ != SQL_NULL_HENV) {
            SQLFreeHandle(SQL_HANDLE_ENV, env_);
        }
    }

    void migrate() override {
        const char* parts[] = {
            "IF OBJECT_ID(N'persons', N'U') IS NULL CREATE TABLE persons ("
            "id BIGINT IDENTITY(1,1) PRIMARY KEY, full_name NVARCHAR(200) NOT NULL, job_title NVARCHAR(120) NOT NULL)",
            "IF OBJECT_ID(N'assets', N'U') IS NULL CREATE TABLE assets ("
            "id BIGINT IDENTITY(1,1) PRIMARY KEY, inv_no NVARCHAR(20) NOT NULL UNIQUE, title NVARCHAR(200) NOT NULL, "
            "person_id BIGINT NOT NULL REFERENCES persons(id), kind INT NOT NULL, status INT NOT NULL, "
            "method INT NOT NULL CHECK (method BETWEEN 1 AND 4), "
            "accepted_on CHAR(10) NOT NULL, cost_kop BIGINT NOT NULL CHECK (cost_kop >= 0), "
            "liq_kop BIGINT NOT NULL CHECK (liq_kop >= 0), life_months INT NOT NULL, decline_x100 INT NOT NULL, "
            "plan_units BIGINT NOT NULL CONSTRAINT df_plan DEFAULT 0, place NVARCHAR(200) NOT NULL CONSTRAINT df_place DEFAULT N'', "
            "note NVARCHAR(400) NOT NULL CONSTRAINT df_note DEFAULT N'', retired_on CHAR(10) NULL, retire_reason NVARCHAR(300) NULL)",
            "IF OBJECT_ID(N'charges', N'U') IS NULL CREATE TABLE charges ("
            "id BIGINT IDENTITY(1,1) PRIMARY KEY, asset_id BIGINT NOT NULL REFERENCES assets(id), "
            "y INT NOT NULL, m INT NOT NULL CHECK (m BETWEEN 1 AND 12), amount_kop BIGINT NOT NULL, "
            "book_kop BIGINT NOT NULL, units BIGINT NOT NULL CONSTRAINT df_units DEFAULT 0, "
            "CONSTRAINT uq_charge UNIQUE (asset_id, y, m))",
            "IF OBJECT_ID(N'audit_log', N'U') IS NULL CREATE TABLE audit_log ("
            "id BIGINT IDENTITY(1,1) PRIMARY KEY, ts DATETIME2 NOT NULL CONSTRAINT df_ts DEFAULT SYSDATETIME(), "
            "user_name NVARCHAR(80) NOT NULL, action NVARCHAR(80) NOT NULL, details NVARCHAR(400) NOT NULL)",
            "IF OBJECT_ID(N'settings', N'U') IS NULL CREATE TABLE settings ("
            "skey NVARCHAR(40) NOT NULL PRIMARY KEY, svalue NVARCHAR(200) NOT NULL)",
            "IF NOT EXISTS (SELECT 1 FROM sys.indexes WHERE name = N'ix_charges_asset') "
            "CREATE INDEX ix_charges_asset ON charges(asset_id)",
            "IF NOT EXISTS (SELECT 1 FROM sys.indexes WHERE name = N'ix_assets_person') "
            "CREATE INDEX ix_assets_person ON assets(person_id)",
        };
        for (const char* sql : parts) {
            exec(sql);
        }
        exec(
            "CREATE OR ALTER TRIGGER trg_asset_ins ON assets AFTER INSERT AS "
            "BEGIN SET NOCOUNT ON; "
            "INSERT INTO audit_log(user_name, action, details) "
            "SELECT N'триггер', N'карточка создана', inv_no + N' ' + title FROM inserted; END");
    }

    std::string engineName() const override { return "SQL Server"; }

    std::string currentLogin() override {
        std::string name;
        query("SELECT SUSER_SNAME()", {}, [&](SQLHSTMT st) { name = colText(st, 1); });
        return name;
    }

    // Права берутся из ролей СУБД, а не из таблицы приложения.
    // Администратор базы (dbo) тоже может писать, иначе учебный вход Windows упирался бы в пустую роль.
    std::string currentRole() override {
        std::string role;
        query(
            "SELECT CASE WHEN IS_MEMBER(N'db_owner') = 1 THEN N'бухгалтер' "
            "WHEN IS_MEMBER(N'os_accountant') = 1 THEN N'бухгалтер' "
            "WHEN IS_MEMBER(N'os_reader') = 1 THEN N'смотритель' ELSE N'' END",
            {}, [&](SQLHSTMT st) { role = colText(st, 1); });
        return role;
    }

    void resetAll() override {
        exec("DELETE FROM charges");
        exec("DELETE FROM assets");
        exec("DELETE FROM persons");
        exec("DELETE FROM audit_log");
        exec("DELETE FROM settings");
    }

    int64_t addPerson(const Person& p) override {
        int64_t id = 0;
        query("INSERT INTO persons(full_name, job_title) VALUES(?, ?); SELECT CAST(SCOPE_IDENTITY() AS BIGINT)",
              {text(p.name), text(p.post)}, [&](SQLHSTMT st) { id = colBig(st, 1); });
        return id;
    }

    std::vector<Person> people() override {
        std::vector<Person> rows;
        query("SELECT id, full_name, job_title FROM persons ORDER BY id", {}, [&](SQLHSTMT st) {
            Person p;
            p.id = colBig(st, 1);
            p.name = colText(st, 2);
            p.post = colText(st, 3);
            rows.push_back(p);
        });
        return rows;
    }

    std::optional<Person> person(int64_t id) override {
        std::optional<Person> found;
        query("SELECT id, full_name, job_title FROM persons WHERE id = ?", {num(id)}, [&](SQLHSTMT st) {
            Person p;
            p.id = colBig(st, 1);
            p.name = colText(st, 2);
            p.post = colText(st, 3);
            found = p;
        });
        return found;
    }

    int64_t addAsset(const Asset& a) override {
        std::ostringstream iso;
        iso << a.accepted.year << '-' << (a.accepted.month < 10 ? "0" : "") << a.accepted.month << '-'
            << (a.accepted.day < 10 ? "0" : "") << a.accepted.day;
        int64_t id = 0;
        query(
            "INSERT INTO assets(inv_no, title, person_id, kind, status, method, accepted_on, cost_kop, liq_kop, "
            "life_months, decline_x100, plan_units, place, note) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?); "
            "SELECT CAST(SCOPE_IDENTITY() AS BIGINT)",
            {text(a.inv), text(a.title), num(a.personId), num(static_cast<int>(a.kind)),
             num(static_cast<int>(a.status)), num(static_cast<int>(a.method)), text(iso.str()), num(a.costKop),
             num(a.liqKop), num(a.lifeMonths), num(a.declineX100), num(a.planUnits), text(a.place), text(a.note)},
            [&](SQLHSTMT st) { id = colBig(st, 1); });
        return id;
    }

    void retireAsset(int64_t id, const std::string& isoDate, const std::string& reason) override {
        execParams("UPDATE assets SET status = ?, retired_on = ?, retire_reason = ? WHERE id = ?",
                   {num(static_cast<int>(AssetStatus::Retired)), text(isoDate), text(reason), num(id)});
    }

    std::optional<Asset> asset(int64_t id) override {
        std::optional<Asset> found;
        query(std::string("SELECT ") + kCols + " FROM assets WHERE id = ?", {num(id)},
              [&](SQLHSTMT st) { found = readAsset(st); });
        return found;
    }

    std::optional<Asset> assetByInv(const std::string& inv) override {
        std::optional<Asset> found;
        query(std::string("SELECT ") + kCols + " FROM assets WHERE inv_no = ?", {text(inv)},
              [&](SQLHSTMT st) { found = readAsset(st); });
        return found;
    }

    std::vector<Asset> assets() override {
        std::vector<Asset> rows;
        query(std::string("SELECT ") + kCols + " FROM assets ORDER BY id", {},
              [&](SQLHSTMT st) { rows.push_back(readAsset(st)); });
        return rows;
    }

    int64_t addCharge(const Charge& c) override {
        int64_t id = 0;
        query("INSERT INTO charges(asset_id, y, m, amount_kop, book_kop, units) VALUES(?,?,?,?,?,?); "
              "SELECT CAST(SCOPE_IDENTITY() AS BIGINT)",
              {num(c.assetId), num(c.period.year), num(c.period.month), num(c.amountKop), num(c.bookKop), num(c.units)},
              [&](SQLHSTMT st) { id = colBig(st, 1); });
        return id;
    }

    std::vector<Charge> charges(int64_t assetId) override {
        std::vector<Charge> rows;
        query(std::string("SELECT ") + kChargeCols + " FROM charges WHERE asset_id = ? ORDER BY y, m",
              {num(assetId)}, [&](SQLHSTMT st) { rows.push_back(readCharge(st)); });
        return rows;
    }

    std::vector<Charge> chargesAll() override {
        std::vector<Charge> rows;
        query(std::string("SELECT ") + kChargeCols + " FROM charges ORDER BY asset_id, y, m", {},
              [&](SQLHSTMT st) { rows.push_back(readCharge(st)); });
        return rows;
    }

    std::vector<Charge> chargesForPeriod(Period p) override {
        std::vector<Charge> rows;
        query(std::string("SELECT ") + kChargeCols + " FROM charges WHERE y = ? AND m = ? ORDER BY asset_id",
              {num(p.year), num(p.month)}, [&](SQLHSTMT st) { rows.push_back(readCharge(st)); });
        return rows;
    }

    void deleteChargesFor(Period p) override {
        execParams("DELETE FROM charges WHERE y = ? AND m = ?", {num(p.year), num(p.month)});
    }

    bool hasCharge(int64_t assetId, Period p) override {
        bool yes = false;
        query("SELECT 1 FROM charges WHERE asset_id = ? AND y = ? AND m = ?",
              {num(assetId), num(p.year), num(p.month)}, [&](SQLHSTMT) { yes = true; });
        return yes;
    }

    void audit(const std::string& user, const std::string& action, const std::string& details) override {
        execParams("INSERT INTO audit_log(user_name, action, details) VALUES(?,?,?)",
                   {text(user), text(action), text(details)});
    }

    std::vector<std::string> auditTail(int limit) override {
        std::vector<std::string> rows;
        query("SELECT TOP (?) CONVERT(varchar(19), ts, 120), user_name, action, details FROM audit_log "
              "ORDER BY id DESC",
              {num(limit)}, [&](SQLHSTMT st) {
                  if (static_cast<int>(rows.size()) >= limit) {
                      return;
                  }
                  // SQLGetData требует колонки по возрастанию, поэтому никаких
                  // "colText(1) + colText(2) + ..." в одном выражении.
                  std::string ts = colText(st, 1);
                  std::string who = colText(st, 2);
                  std::string action = colText(st, 3);
                  std::string details = colText(st, 4);
                  rows.push_back(ts + " | " + who + " | " + action + " | " + details);
              });
        return rows;
    }

    void setSetting(const std::string& key, const std::string& value) override {
        execParams(
            "MERGE settings AS t USING (SELECT ? AS skey, ? AS svalue) AS s ON t.skey = s.skey "
            "WHEN MATCHED THEN UPDATE SET svalue = s.svalue WHEN NOT MATCHED THEN INSERT (skey, svalue) VALUES (s.skey, s.svalue);",
            {text(key), text(value)});
    }

    std::string setting(const std::string& key, const std::string& fallback) override {
        std::string value = fallback;
        bool hit = false;
        query("SELECT svalue FROM settings WHERE skey = ?", {text(key)}, [&](SQLHSTMT st) {
            value = colText(st, 1);
            hit = true;
            (void)hit;
        });
        return value;
    }

    void backupTo(const std::string& filePath) override {
        std::string dbName = "AssetOs";
        query("SELECT DB_NAME()", {}, [&](SQLHSTMT st) { dbName = colText(st, 1); });
        std::string target = filePath;
        if (target.empty()) {
            std::string dir;
            query("SELECT CAST(SERVERPROPERTY(N'InstanceDefaultBackupPath') AS NVARCHAR(400))", {},
                  [&](SQLHSTMT st) { dir = colText(st, 1); });
            if (dir.empty()) {
                throw std::runtime_error("сервер не подсказал папку резервных копий, укажите путь сами");
            }
            if (dir.back() != '\\') {
                dir.push_back('\\');
            }
            target = dir + dbName + ".bak";
        }
        std::string safe;
        for (char ch : target) {
            if (ch == '\'') {
                safe += "''";
            } else {
                safe.push_back(ch);
            }
        }
        exec("BACKUP DATABASE [" + dbName + "] TO DISK = N'" + safe + "' WITH INIT, COPY_ONLY, STATS = 10");
    }

private:
    struct Param {
        bool isText = false;
        std::wstring wide;
        int64_t number = 0;
    };
    static Param text(const std::string& s) {
        Param p;
        p.isText = true;
        p.wide = toWide(s);
        return p;
    }
    static Param num(int64_t n) {
        Param p;
        p.number = n;
        return p;
    }

    static const char* kCols;
    static const char* kChargeCols;

    struct StmtGuard {
        SQLHSTMT h = SQL_NULL_HSTMT;
        ~StmtGuard() {
            if (h != SQL_NULL_HSTMT) {
                SQLFreeHandle(SQL_HANDLE_STMT, h);
            }
        }
    };

    void exec(const std::string& sql) { execParams(sql, {}); }

    static bool ok(SQLRETURN rc) {
        return SQL_SUCCEEDED(rc) || rc == SQL_NO_DATA;
    }

    void execParams(const std::string& sql, const std::vector<Param>& params) {
        StmtGuard guard;
        if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &guard.h))) {
            throw std::runtime_error(diag(SQL_HANDLE_DBC, dbc_));
        }
        SQLHSTMT st = guard.h;
        std::vector<int64_t> numbers(params.size());
        std::vector<SQLLEN> ind(params.size(), 0);
        std::wstring wideSql = toWide(sql);
        if (!params.empty()) {
            if (!SQL_SUCCEEDED(SQLPrepareW(st, reinterpret_cast<SQLWCHAR*>(&wideSql[0]), SQL_NTS))) {
                throw std::runtime_error(diag(SQL_HANDLE_STMT, st));
            }
            for (size_t i = 0; i < params.size(); ++i) {
                SQLRETURN rc;
                if (params[i].isText) {
                    ind[i] = SQL_NTS;
                    rc = SQLBindParameter(st, static_cast<SQLUSMALLINT>(i + 1), SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,
                                          static_cast<SQLULEN>(params[i].wide.size()), 0,
                                          const_cast<SQLWCHAR*>(params[i].wide.c_str()), 0, &ind[i]);
                } else {
                    numbers[i] = params[i].number;
                    ind[i] = 0;
                    rc = SQLBindParameter(st, static_cast<SQLUSMALLINT>(i + 1), SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT,
                                          0, 0, &numbers[i], 0, &ind[i]);
                }
                if (!SQL_SUCCEEDED(rc)) {
                    throw std::runtime_error(diag(SQL_HANDLE_STMT, st));
                }
            }
            if (!ok(SQLExecute(st))) {
                throw std::runtime_error(diag(SQL_HANDLE_STMT, st) + " | " + sql);
            }
        } else if (!ok(SQLExecDirectW(st, reinterpret_cast<SQLWCHAR*>(&wideSql[0]), SQL_NTS))) {
            throw std::runtime_error(diag(SQL_HANDLE_STMT, st) + " | " + sql);
        }
    }

    template <typename Fn>
    void query(const std::string& sql, const std::vector<Param>& params, Fn fn) {
        StmtGuard guard;
        if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &guard.h))) {
            throw std::runtime_error(diag(SQL_HANDLE_DBC, dbc_));
        }
        SQLHSTMT st = guard.h;
        std::vector<int64_t> numbers(params.size());
        std::vector<SQLLEN> ind(params.size(), 0);
        std::wstring wideSql = toWide(sql);
        if (!SQL_SUCCEEDED(SQLPrepareW(st, reinterpret_cast<SQLWCHAR*>(&wideSql[0]), SQL_NTS))) {
            throw std::runtime_error(diag(SQL_HANDLE_STMT, st));
        }
        for (size_t i = 0; i < params.size(); ++i) {
            SQLRETURN rc;
            if (params[i].isText) {
                ind[i] = SQL_NTS;
                rc = SQLBindParameter(st, static_cast<SQLUSMALLINT>(i + 1), SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,
                                      static_cast<SQLULEN>(params[i].wide.size()), 0,
                                      const_cast<SQLWCHAR*>(params[i].wide.c_str()), 0, &ind[i]);
            } else {
                numbers[i] = params[i].number;
                rc = SQLBindParameter(st, static_cast<SQLUSMALLINT>(i + 1), SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0,
                                      0, &numbers[i], 0, &ind[i]);
            }
            if (!SQL_SUCCEEDED(rc)) {
                throw std::runtime_error(diag(SQL_HANDLE_STMT, st));
            }
        }
        if (!ok(SQLExecute(st))) {
            throw std::runtime_error(diag(SQL_HANDLE_STMT, st) + " | " + sql);
        }
        // В пакете "INSERT ...; SELECT SCOPE_IDENTITY()" первый набор строк — это
        // только счётчик вставки, значение лежит во втором. Поэтому перебираю наборы.
        while (true) {
            SQLSMALLINT cols = 0;
            SQLNumResultCols(st, &cols);
            if (cols > 0) {
                while (true) {
                    SQLRETURN rc = SQLFetch(st);
                    if (rc == SQL_NO_DATA) {
                        break;
                    }
                    if (!SQL_SUCCEEDED(rc)) {
                        throw std::runtime_error(diag(SQL_HANDLE_STMT, st));
                    }
                    fn(st);
                }
            }
            SQLRETURN more = SQLMoreResults(st);
            if (more == SQL_NO_DATA) {
                break;
            }
            if (!SQL_SUCCEEDED(more)) {
                throw std::runtime_error(diag(SQL_HANDLE_STMT, st));
            }
        }
    }

    static int64_t colBig(SQLHSTMT st, SQLUSMALLINT i) {
        int64_t v = 0;
        SQLLEN ind = 0;
        SQLRETURN rc = SQLGetData(st, i, SQL_C_SBIGINT, &v, sizeof(v), &ind);
        if (!SQL_SUCCEEDED(rc) || ind == SQL_NULL_DATA) {
            return 0;
        }
        return v;
    }

    static std::string colText(SQLHSTMT st, SQLUSMALLINT i) {
        SQLWCHAR buf[4096];
        SQLLEN ind = 0;
        SQLRETURN rc = SQLGetData(st, i, SQL_C_WCHAR, buf, sizeof(buf), &ind);
        if (!SQL_SUCCEEDED(rc) || ind == SQL_NULL_DATA) {
            return {};
        }
        size_t chars = static_cast<size_t>(ind) / sizeof(SQLWCHAR);
        if (chars > sizeof(buf) / sizeof(buf[0]) - 1) {
            chars = sizeof(buf) / sizeof(buf[0]) - 1;
        }
        return toUtf8(std::wstring(buf, buf + chars));
    }

    static Asset readAsset(SQLHSTMT st) {
        Asset a;
        a.id = colBig(st, 1);
        a.inv = colText(st, 2);
        a.title = colText(st, 3);
        a.personId = colBig(st, 4);
        a.kind = static_cast<AssetKind>(colBig(st, 5));
        a.status = static_cast<AssetStatus>(colBig(st, 6));
        a.method = static_cast<Method>(colBig(st, 7));
        std::string iso = colText(st, 8);
        if (iso.size() >= 10) {
            a.accepted.year = std::stoi(iso.substr(0, 4));
            a.accepted.month = std::stoi(iso.substr(5, 2));
            a.accepted.day = std::stoi(iso.substr(8, 2));
        }
        a.costKop = colBig(st, 9);
        a.liqKop = colBig(st, 10);
        a.lifeMonths = static_cast<int>(colBig(st, 11));
        a.declineX100 = static_cast<int>(colBig(st, 12));
        a.planUnits = colBig(st, 13);
        a.place = colText(st, 14);
        a.note = colText(st, 15);
        a.retiredOn = colText(st, 16);
        a.retireReason = colText(st, 17);
        return a;
    }

    static Charge readCharge(SQLHSTMT st) {
        Charge c;
        c.id = colBig(st, 1);
        c.assetId = colBig(st, 2);
        c.period.year = static_cast<int>(colBig(st, 3));
        c.period.month = static_cast<int>(colBig(st, 4));
        c.amountKop = colBig(st, 5);
        c.bookKop = colBig(st, 6);
        c.units = colBig(st, 7);
        return c;
    }

    std::string connection_;
    SQLHENV env_ = SQL_NULL_HENV;
    SQLHDBC dbc_ = SQL_NULL_HDBC;
};

const char* SqlServerStore::kCols =
    "id, inv_no, title, person_id, kind, status, method, accepted_on, cost_kop, liq_kop, life_months, "
    "decline_x100, plan_units, place, note, retired_on, retire_reason";

const char* SqlServerStore::kChargeCols = "id, asset_id, y, m, amount_kop, book_kop, units";

}  // namespace

std::unique_ptr<Store> openSqlServer(const std::string& connectionString) {
    return std::make_unique<SqlServerStore>(connectionString);
}

}  // namespace osmod
