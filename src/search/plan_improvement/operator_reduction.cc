#include "operator_reduction.h"

#include "../operator_id.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../task_utils/task_properties.h"

#include <iostream>
#include <set>

using namespace std;
using namespace task_properties;

Plan OperatorReduction::improve(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    cout << "arrived at operator reduction" << "\n";
    /*
    This improvement idea should take the existing task and reduce the operators
    it can actively use in its search
    */
    return plan;
}