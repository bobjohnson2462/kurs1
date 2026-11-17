#pragma once

#include "osmod/model.h"

#include <cstdint>
#include <string>

namespace osmod {

// Сколько способов знает модуль. Меню и проверки берут число отсюда,
// чтобы новый способ добавлялся в одном месте.
constexpr int kMethodCount = 4;

// Данные одного шага начисления. Копейки целые, месяц уже известен.
struct DepreciationInput {
    int64_t costKop = 0;
    int64_t liqKop = 0;
    int64_t accumulatedKop = 0;
    int64_t planUnits = 0;
    int64_t accumulatedUnits = 0;
    int64_t unitsThisPeriod = 0;
    int lifeMonths = 0;
    int declineX100 = 200;
    int monthsAlready = 0;
    Method method = Method::Linear;
    bool forceRemainder = false;
};

struct DepreciationOutput {
    int64_t amountKop = 0;
    int64_t bookKop = 0;
    bool reachedFloor = false;
    std::string note;
};

struct StrategyResult {
    int64_t amountKop = 0;
    std::string note;
};

// Паттерн «Стратегия»: каждый способ амортизации — отдельный класс.
// Точка вариации: новый способ = класс + ветка в strategyFor().
class DepreciationStrategy {
public:
    virtual ~DepreciationStrategy() = default;
    virtual const char* name() const = 0;
    virtual StrategyResult monthlyAmount(const DepreciationInput& in) const = 0;
};

// Единственное место, где перечислены все способы.
const DepreciationStrategy& strategyFor(Method method);

// Общее правило месяца: последний месяц забирает остаток,
// сумма не уходит ниже ликвидационной стоимости.
DepreciationOutput chargeDepreciation(const DepreciationInput& in);

}  // namespace osmod
