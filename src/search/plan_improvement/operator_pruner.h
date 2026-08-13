#ifndef PLAN_IMPROVEMENT_OPERATOR_PRUNER_H
#define PLAN_IMPROVEMENT_OPERATOR_PRUNER_H

#include "../component.h"
#include "../plan_manager.h"
#include "../pruning_method.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// Exact OperatorID-based pruning
// ---------------------------------------------------------------------------

class OperatorPruner : public PruningMethod {
    std::unordered_set<int> allowed_operator_ids;

protected:
    void prune(const State &state, std::vector<OperatorID> &op_ids) override;

public:
    OperatorPruner(
        const std::shared_ptr<AbstractTask> &task, utils::Verbosity verbosity,
        const Plan &plan);

    bool is_safe() const override {
        return false;
    }
};

class TaskIndependentOperatorPruner
    : public components::TaskIndependentComponent<PruningMethod> {
    utils::Verbosity verbosity;
    Plan plan;

protected:
    std::shared_ptr<PruningMethod> create_task_specific_component(
        const std::shared_ptr<AbstractTask> &task) const override;

public:
    TaskIndependentOperatorPruner(utils::Verbosity verbosity, const Plan &plan);
};

// ---------------------------------------------------------------------------
// Operator-name/schema-based pruning
// ---------------------------------------------------------------------------

class OperatorNamePruner : public PruningMethod {
    std::unordered_set<std::string> allowed_operator_names;

    std::string get_operator_schema_name(
        const std::string &grounded_name) const;

protected:
    void prune(const State &state, std::vector<OperatorID> &op_ids) override;

public:
    OperatorNamePruner(
        const std::shared_ptr<AbstractTask> &task, utils::Verbosity verbosity,
        const Plan &plan);

    bool is_safe() const override {
        return false;
    }
};

class TaskIndependentOperatorNamePruner
    : public components::TaskIndependentComponent<PruningMethod> {
    utils::Verbosity verbosity;
    Plan plan;

protected:
    std::shared_ptr<PruningMethod> create_task_specific_component(
        const std::shared_ptr<AbstractTask> &task) const override;

public:
    TaskIndependentOperatorNamePruner(
        utils::Verbosity verbosity, const Plan &plan);
};

#endif