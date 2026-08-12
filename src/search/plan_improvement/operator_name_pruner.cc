#include "operator_pruner.h"

#include <memory>
#include <utility>

using namespace std;

OperatorPruner::OperatorPruner(
    const shared_ptr<AbstractTask> &task, utils::Verbosity verbosity,
    const Plan &plan)
    : PruningMethod(task, verbosity) {
    for (OperatorID operator_id : plan) {
        allowed_operator_ids.insert(operator_id.get_index());
    }
}

void OperatorPruner::prune(const State &, vector<OperatorID> &op_ids) {
    vector<OperatorID> remaining_operators;

    for (OperatorID operator_id : op_ids) {
        if (allowed_operator_ids.find(operator_id.get_index()) !=
            allowed_operator_ids.end()) {
            remaining_operators.push_back(operator_id);
        }
    }

    op_ids = move(remaining_operators);
}

TaskIndependentOperatorPruner::TaskIndependentOperatorPruner(
    utils::Verbosity verbosity, const Plan &plan)
    : verbosity(verbosity), plan(plan) {
}

shared_ptr<PruningMethod>
TaskIndependentOperatorPruner::create_task_specific_component(
    const shared_ptr<AbstractTask> &task) const {
    return make_shared<OperatorPruner>(task, verbosity, plan);
}