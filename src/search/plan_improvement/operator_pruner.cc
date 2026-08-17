#include "operator_pruner.h"

#include "../task_proxy.h"

#include <memory>
#include <string>
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

string OperatorNamePruner::get_operator_schema_name(
    const string &grounded_name) const {
    size_t start = 0;
    if (!grounded_name.empty() && grounded_name[0] == '(') {
        start = 1;
    }
    size_t end = grounded_name.find(' ', start);
    // AE: Thänks Chattyboi for pointing out this missing
    if (end == string::npos) {
        end = grounded_name.find(')', start);
    }

    if (end == string::npos) {
        end = grounded_name.size();
    }

    return grounded_name.substr(start, end - start);
}

OperatorNamePruner::OperatorNamePruner(
    const shared_ptr<AbstractTask> &task, utils::Verbosity verbosity,
    const Plan &plan)
    : PruningMethod(task, verbosity) {
    OperatorsProxy operators = task_proxy.get_operators();

    for (OperatorID operator_id : plan) {
        OperatorProxy op = operators[operator_id];
        string operator_name = get_operator_schema_name(op.get_name());
        allowed_operator_names.insert(operator_name);
    }
}

void OperatorNamePruner::prune(const State &, vector<OperatorID> &op_ids) {
    OperatorsProxy operators = task_proxy.get_operators();

    vector<OperatorID> remaining_operators;

    for (OperatorID operator_id : op_ids) {
        OperatorProxy op = operators[operator_id];

        string operator_name = get_operator_schema_name(op.get_name());

        if (allowed_operator_names.find(operator_name) !=
            allowed_operator_names.end()) {
            remaining_operators.push_back(operator_id);
        }
    }

    op_ids = move(remaining_operators);
}

TaskIndependentOperatorNamePruner::TaskIndependentOperatorNamePruner(
    utils::Verbosity verbosity, const Plan &plan)
    : verbosity(verbosity), plan(plan) {
}

shared_ptr<PruningMethod>
TaskIndependentOperatorNamePruner::create_task_specific_component(
    const shared_ptr<AbstractTask> &task) const {
    return make_shared<OperatorNamePruner>(task, verbosity, plan);
}