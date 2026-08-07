#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

#include "search_algorithm.h"

#include <memory>
#include <string>

// AE: added
class PlanImprover;

struct ParsedSearchOptions {
    std::shared_ptr<TaskIndependentSearchAlgorithm> search_algorithm;
    // AE: added
    std::shared_ptr<PlanImprover> plan_improver;

    std::string plan_filename;
    int num_previously_generated_plans;
    bool is_part_of_anytime_portfolio;
};

extern ParsedSearchOptions parse_cmd_line(
    int argc, const char **argv, bool is_unit_cost);

extern std::string get_revision_info();
extern std::string get_usage(const std::string &progname);

#endif
