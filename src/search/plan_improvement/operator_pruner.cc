#include "operator_pruner.h"

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