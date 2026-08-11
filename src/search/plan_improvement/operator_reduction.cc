#include "operator_reduction.h"

#include "operator_pruner.h"

#include "../state_registry.h"

#include <iostream>
#include <memory>

using namespace std;

Plan OperatorReduction::improve(
    const Plan &plan, const shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    cout << "Arrived at operator reduction\n";

    shared_ptr<OperatorPruner> operator_pruner =
        make_shared<OperatorPruner>(task, utils::Verbosity::NORMAL, plan);

    // TODO:
    // Run a new optimal search and pass operator_pruner
    // as the pruning method.

    return plan;
}