#pragma once

#include "osmod/service.h"

#include <ostream>
#include <string>
#include <vector>

namespace osmod {
namespace ui {

void printSummary(std::ostream& out, const Service& svc, const std::string& engine, const std::string& company,
                  const std::string& role);
void printPeople(std::ostream& out, Store& store);
void printSheet(std::ostream& out, const Service& svc);
void printCard(std::ostream& out, const Service& svc, int64_t assetId);
void printCloseResult(std::ostream& out, const std::vector<CloseLine>& lines, bool showSkips = true);
void printJournal(std::ostream& out, const Service& svc, int limit);
void printForecast(std::ostream& out, const Forecast& forecast);
void printPeriodReport(std::ostream& out, const PeriodReport& report);
void printYearTotals(std::ostream& out, const YearTotals& totals);
void printMethods(std::ostream& out);

}  // namespace ui
}  // namespace osmod
