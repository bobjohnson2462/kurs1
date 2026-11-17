#pragma once

#include "osmod/model.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace osmod {

class Store {
public:
    virtual ~Store() = default;
    virtual void migrate() = 0;
    virtual std::string engineName() const = 0;
    virtual std::string currentLogin() = 0;
    virtual std::string currentRole() = 0;
    virtual void resetAll() = 0;

    virtual int64_t addPerson(const Person& p) = 0;
    virtual std::vector<Person> people() = 0;
    virtual std::optional<Person> person(int64_t id) = 0;

    virtual int64_t addAsset(const Asset& a) = 0;
    virtual void retireAsset(int64_t id, const std::string& isoDate, const std::string& reason) = 0;
    virtual std::optional<Asset> asset(int64_t id) = 0;
    virtual std::optional<Asset> assetByInv(const std::string& inv) = 0;
    virtual std::vector<Asset> assets() = 0;

    virtual int64_t addCharge(const Charge& c) = 0;
    virtual std::vector<Charge> charges(int64_t assetId) = 0;
    virtual std::vector<Charge> chargesAll() = 0;
    virtual std::vector<Charge> chargesForPeriod(Period p) = 0;
    virtual void deleteChargesFor(Period p) = 0;
    virtual bool hasCharge(int64_t assetId, Period p) = 0;

    virtual void audit(const std::string& user, const std::string& action, const std::string& details) = 0;
    virtual std::vector<std::string> auditTail(int limit) = 0;

    virtual void setSetting(const std::string& key, const std::string& value) = 0;
    virtual std::string setting(const std::string& key, const std::string& fallback) = 0;

    virtual void backupTo(const std::string& filePath) = 0;
};

std::unique_ptr<Store> openSqlServer(const std::string& connectionString);

}  // namespace osmod
