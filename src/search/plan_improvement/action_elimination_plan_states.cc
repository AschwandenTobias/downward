#include "action_elimination_plan_states.h"

#include "../operator_id.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../task_utils/task_properties.h"

#include <set>
#include <vector>

using namespace std;
using namespace task_properties;

struct StateInfo {
    State state;
    size_t index;
    size_t prefix_cost;
};

vector<StateInfo> extract_state_info_from_plan(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    OperatorsProxy operators = task_proxy.get_operators();
    State initial_state = state_registry.get_initial_state();
    // Use a vector to store all infos about each state in the original plan.
    vector<StateInfo> plan_states;
    // Starts with the initial state and iterates through the plan
    State map_state = initial_state;
    size_t prefix_cost = 0;
    for (size_t i = 0; i < plan.size(); i++) {
        plan_states.push_back({map_state, i, prefix_cost});
        OperatorProxy op = operators[plan.at(i)];
        prefix_cost += op.get_cost();
        map_state = state_registry.get_successor_state(map_state, op);
    }
    plan_states.push_back({map_state, plan.size(), prefix_cost});
    return plan_states;
}

Plan ActionEliminationPlanStates::improve(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    Plan current_plan = plan;
    OperatorsProxy operators = task_proxy.get_operators();

    bool improvement_found = true;
    cout << "Trying now additional reductions" << "\n";
    while (improvement_found) {
        improvement_found = false;
        vector<StateInfo> plan_states = extract_state_info_from_plan(
            current_plan, task_proxy, state_registry);
        for (size_t i = 0; i < current_plan.size(); ++i) {
            State simulated_state = plan_states[i].state;
            size_t simulated_cost = plan_states[i].prefix_cost;

            Plan candidate_segment;

            for (size_t j = i + 1; j < current_plan.size(); ++j) {
                OperatorProxy op = operators[current_plan[j]];

                if (!is_applicable(op, simulated_state)) {
                    continue;
                }

                simulated_state =
                    state_registry.get_successor_state(simulated_state, op);

                simulated_cost += op.get_cost();
                candidate_segment.push_back(current_plan[j]);

                for (size_t k = i + 1; k < plan_states.size(); ++k) {
                    if (simulated_state != plan_states[k].state) {
                        continue;
                    }
                    cout << "Am on a node from the original plan" << "\n";
                    if (simulated_cost >= plan_states[k].prefix_cost) {
                        cout << "But simulated cost: " << simulated_cost
                             << ", was higher than prefix cost: "
                             << plan_states[k].prefix_cost << "\n";
                        continue;
                    }

                    Plan improved_plan;

                    improved_plan.insert(
                        improved_plan.end(), current_plan.begin(),
                        current_plan.begin() + i);

                    improved_plan.insert(
                        improved_plan.end(), candidate_segment.begin(),
                        candidate_segment.end());

                    improved_plan.insert(
                        improved_plan.end(), current_plan.begin() + k,
                        current_plan.end());

                    cout << "Found cheaper reconnection.\n"
                         << "Start action index: " << i << "\n"
                         << "Matched state index: " << k << "\n"
                         << "Old prefix cost: " << plan_states[k].prefix_cost
                         << "\n"
                         << "New prefix cost: " << simulated_cost << "\n"
                         << "Old plan length: " << current_plan.size() << "\n"
                         << "New plan length: " << improved_plan.size() << "\n";

                    current_plan = std::move(improved_plan);
                    improvement_found = true;
                    break;
                }

                if (improvement_found) {
                    break;
                }
            }
            if (improvement_found) {
                break;
            }
        }
    }
    return current_plan;
}