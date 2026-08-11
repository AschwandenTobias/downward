#ifndef PLAN_IMPROVEMENT_OPERATOR_PRUNER_H
#define PLAN_IMPROVEMENT_OPERATOR_PRUNER_H

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

#endif