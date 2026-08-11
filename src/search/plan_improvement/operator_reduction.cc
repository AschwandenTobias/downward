#include "operator_reduction.h"

#include "../operator_id.h"
#include "../pruning_method.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../task_utils/task_properties.h"

#include <iostream>
#include <set>

using namespace std;
using namespace task_properties;

Plan OperatorReduction::improve(
    const Plan &plan, const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    cout << "arrived at operator reduction" << "\n";
    /*
    This improvement idea should take the existing task and reduce the operators
    it can actively use in its search
    */
    TaskProxy task_proxy(*task);
    return plan;
}