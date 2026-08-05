#ifndef ACTION_ELIMINATION_H
#define ACTION_ELIMINATION_H

#include "../operator_id.h"
#include "../plan_manager.h"
#include "../state_registry.h"

#include "../task_utils/task_properties.h"

Plan action_elimination(
    const Plan &plan, const TaskProxy &taskProxy,
    StateRegistry &state_registry);
Plan repeated_state_elimination(
    const Plan &plan, const TaskProxy &taskProxy,
    StateRegistry &state_registry);

#endif