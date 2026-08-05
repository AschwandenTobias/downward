#ifndef ACTION_ELIMINATION_H
#define ACTION_ELIMINATION_H

#include "../operator_id.h"
#include "../plan_manager.h"
#include "../state_registry.h"

#include "../task_utils/task_properties.h"
struct StateInfo {
    State state;
    size_t index;
    size_t prefix_cost;
};

Plan action_elimination(
    const Plan &plan, const TaskProxy &taskProxy,
    StateRegistry &state_registry);
Plan action_elimination_track_plan_states(
    const Plan &plan, const TaskProxy &taskProxy,
    StateRegistry &state_registry);
std::vector<StateInfo> extract_state_info_from_plan(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry);

#endif