#include "osmod/depreciation.h"

#include "osmod/money.h"

#include <stdexcept>

namespace osmod {
namespace {

int64_t depreciableBase(const DepreciationInput& in) {
    return in.costKop - in.liqKop;
}

int64_t remaining(const DepreciationInput& in) {
    int64_t left = depreciableBase(in) - in.accumulatedKop;
    return left < 0 ? 0 : left;
}

class LinearStrategy : public DepreciationStrategy {
public:
    const char* name() const override { return "линейный"; }

    StrategyResult monthlyAmount(const DepreciationInput& in) const override {
        if (in.lifeMonths <= 0) {
            throw std::runtime_error("для линейного способа нужен срок в месяцах");
        }
        StrategyResult result;
        result.amountKop = depreciableBase(in) / in.lifeMonths;
        result.note = "равная доля амортизируемой суммы";
        return result;
    }
};

class DecliningStrategy : public DepreciationStrategy {
public:
    const char* name() const override { return "уменьшаемого остатка"; }

    StrategyResult monthlyAmount(const DepreciationInput& in) const override {
        if (in.lifeMonths <= 0 || in.declineX100 <= 0) {
            throw std::runtime_error("для уменьшаемого остатка нужны срок и коэффициент");
        }
        int64_t book = in.costKop - in.accumulatedKop;
        StrategyResult result;
        result.amountKop = mulDivFloor(book, in.declineX100, static_cast<int64_t>(in.lifeMonths) * 100);
        if (result.amountKop < 1) {
            result.amountKop = 1;
        }
        result.note = "процент от текущей балансовой стоимости";
        return result;
    }
};

class OutputStrategy : public DepreciationStrategy {
public:
    const char* name() const override { return "пропорционально выработке"; }

    StrategyResult monthlyAmount(const DepreciationInput& in) const override {
        if (in.planUnits <= 0) {
            throw std::runtime_error("для способа по выработке нужен план в целых единицах");
        }
        if (in.unitsThisPeriod < 0) {
            throw std::runtime_error("выработка не бывает отрицательной");
        }
        if (in.accumulatedUnits > in.planUnits) {
            throw std::runtime_error("накопленная выработка уже больше плана");
        }
        int64_t room = in.planUnits - in.accumulatedUnits;
        if (in.unitsThisPeriod > room) {
            throw std::runtime_error("выработка больше оставшегося плана, поправьте часы");
        }
        StrategyResult result;
        if (in.unitsThisPeriod == 0) {
            result.note = "выработки не было, за месяц ноль";
            return result;
        }
        if (in.accumulatedUnits + in.unitsThisPeriod == in.planUnits) {
            result.amountKop = remaining(in);
            result.note = "план выработки закрыт, забираю остаток";
            return result;
        }
        result.amountKop = mulDivFloor(depreciableBase(in), in.unitsThisPeriod, in.planUnits);
        result.note = "доля выработки от амортизируемой суммы";
        return result;
    }
};

// Сумма чисел лет: месячный вес (life - monthsAlready) из суммы 1..life.
class SumOfYearsStrategy : public DepreciationStrategy {
public:
    const char* name() const override { return "по сумме чисел лет"; }

    StrategyResult monthlyAmount(const DepreciationInput& in) const override {
        if (in.lifeMonths <= 0) {
            throw std::runtime_error("для способа по сумме чисел лет нужен срок");
        }
        StrategyResult result;
        int64_t weight = static_cast<int64_t>(in.lifeMonths - in.monthsAlready);
        if (weight <= 0) {
            result.amountKop = remaining(in);
            result.note = "срок вышел, добираю остаток";
            return result;
        }
        int64_t denominator = static_cast<int64_t>(in.lifeMonths) * (in.lifeMonths + 1);
        result.amountKop = mulDivFloor(depreciableBase(in), weight * 2, denominator);
        result.note = "вес года в сумме чисел лет срока";
        return result;
    }
};

}  // namespace

const DepreciationStrategy& strategyFor(Method method) {
    static const LinearStrategy linear;
    static const DecliningStrategy declining;
    static const OutputStrategy output;
    static const SumOfYearsStrategy sumOfYears;
    switch (method) {
        case Method::Linear:
            return linear;
        case Method::Declining:
            return declining;
        case Method::Output:
            return output;
        case Method::SumOfYears:
            return sumOfYears;
    }
    throw std::runtime_error("неизвестный способ амортизации");
}

DepreciationOutput chargeDepreciation(const DepreciationInput& in) {
    if (in.costKop < 0 || in.liqKop < 0) {
        throw std::runtime_error("стоимость и ликвидационная не могут быть отрицательными");
    }
    if (in.liqKop > in.costKop) {
        throw std::runtime_error("ликвидационная выше первоначальной, так объект не принимаю");
    }
    if (in.accumulatedKop < 0 || in.monthsAlready < 0) {
        throw std::runtime_error("накопленная амортизация испорчена");
    }
    if (in.accumulatedKop > in.costKop) {
        throw std::runtime_error("накоплено больше первоначальной стоимости");
    }

    DepreciationOutput out;
    int64_t bookNow = in.costKop - in.accumulatedKop;
    int64_t left = remaining(in);
    if (left == 0) {
        out.bookKop = bookNow;
        out.reachedFloor = true;
        out.note = "балансовая уже упёрлась в ликвидационную";
        return out;
    }

    // Последний месяц срока закрывает хвост, чтобы сумма сошлась ровно.
    bool finite = in.method != Method::Output;
    bool lastWindow = finite && in.lifeMonths > 0 && in.monthsAlready >= in.lifeMonths - 1;
    int64_t amount = 0;
    if (in.forceRemainder || lastWindow) {
        amount = left;
        out.note = "последний месяц срока, забираю остаток выше ликвидационной";
    } else {
        StrategyResult got = strategyFor(in.method).monthlyAmount(in);
        amount = got.amountKop;
        out.note = got.note;
    }

    if (amount > left) {
        amount = left;
        if (!out.note.empty()) {
            out.note += "; ";
        }
        out.note += "обрезал по ликвидационной";
    }
    if (amount < 0) {
        amount = 0;
    }

    out.amountKop = amount;
    out.bookKop = bookNow - amount;
    out.reachedFloor = out.bookKop <= in.liqKop;
    return out;
}

}  // namespace osmod
