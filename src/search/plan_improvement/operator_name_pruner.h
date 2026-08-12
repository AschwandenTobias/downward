#ifndef PLAN_IMPROVEMENT_OPERATOR_PRUNER_H
#define PLAN_IMPROVEMENT_OPERATOR_PRUNER_H

#include "../component.h"
#include "../plan_manager.h"
#include "../pruning_method.h"

#include <memory>
#include <unordered_set>
#include <vector>

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

/*
 * Task-independent wrapper for OperatorPruner.
 *
 * We cannot use make_auto_task_independent_component directly
 * because Plan is vector<OperatorID>, and OperatorID is not a
 * supported automatically bindable type.
 */
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

#endif