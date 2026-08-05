#include "action_elimination.h"

#include <iostream>
#include <set>

using namespace std;
using namespace task_properties;

Plan action_elimination(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    Plan reduced_plan = plan;
    cout << "action_elimination is called\n";
    OperatorsProxy operators = task_proxy.get_operators();
    State initial_state = state_registry.get_initial_state();
    // Stores the indices of all marked actions.
    set<size_t> marked_actions;
    State prefix_state = initial_state;
    // AE: This is only true for action costs of 1. Remove later since its not
    // necessary. Didn't find a corresponding function rn.
    size_t plan_cost = plan.size();
    cout << "Plan size in the beginning: " << plan_cost << "\n";
    size_t i = 0;
    while (i < reduced_plan.size()) {
        marked_actions.clear();
        State current_state = prefix_state;
        OperatorProxy prefix_operator = operators[reduced_plan.at(i)];
        // cout << "Try to remove action and simulate rest of plan: " << i <<
        // "\n";
        marked_actions.insert(i);
        for (size_t j = i + 1; j < reduced_plan.size(); j++) {
            // cout << "Check first if a_j is applicable in current_state" <<
            // "\n";
            OperatorProxy current_operator = operators[reduced_plan.at(j)];
            if (is_applicable(current_operator, current_state)) {
                // cout << "Action was applicable, apply it and check next one"
                // << "\n";
                current_state = state_registry.get_successor_state(
                    current_state, current_operator);
            } else {
                // cout
                //<< "Action was not applicable, mark it and try the next one"
                //<< "\n";
                marked_actions.insert(j);
            }
        }
        if (is_goal_state(task_proxy, current_state)) {
            cout << "FOUND A REDUCTION! Remove marked actions from plan"
                 << "\n";
            Plan improved_plan;
            for (size_t i = 0; i < reduced_plan.size(); i++) {
                if (!marked_actions.contains(i)) {
                    improved_plan.push_back(reduced_plan[i]);
                }
            }
            reduced_plan = improved_plan;
        } else {
            prefix_state = state_registry.get_successor_state(
                prefix_state, prefix_operator);
            /*cout
                << "unmark all actions and apply action i to get a new
               current state"
                << "\n";
            */
            i++;
        }
    }
    return reduced_plan;
}

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

Plan action_elimination_track_plan_states(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    Plan reduced_plan = plan;
    OperatorsProxy operators = task_proxy.get_operators();
    State initial_state = state_registry.get_initial_state();
    // Stores the indices of all marked actions.
    set<size_t> marked_actions;
    State prefix_state = initial_state;

    vector<StateInfo> plan_states =
        extract_state_info_from_plan(plan, task_proxy, state_registry);

    size_t i = 0;
    while (i < reduced_plan.size()) {
        marked_actions.clear();
        State current_state = prefix_state;
        OperatorProxy prefix_operator = operators[reduced_plan.at(i)];
        marked_actions.insert(i);
        size_t current_cost = plan_states.at(i).prefix_cost;
        for (size_t j = i + 1; j < reduced_plan.size(); j++) {
            OperatorProxy current_operator = operators[reduced_plan.at(j)];
            if (is_applicable(current_operator, current_state)) {
                current_state = state_registry.get_successor_state(
                    current_state, current_operator);
                current_cost += current_operator.get_cost();
                // We check here if we reached a state in the original plan at a
                // cheaper prize.
                for (size_t k = j + 2; k < plan_states.size(); k++) {
                    if ((current_state == plan_states.at(k).state) &&
                        (current_cost < plan_states.at(k).prefix_cost)) {
                        cout
                            << "Reached a state of the original plan with cheaper cost, current_cost is:"
                            << current_cost << ", prefix_cost is: "
                            << plan_states.at(k).prefix_cost << "\n";
                    }
                }
            } else {
                marked_actions.insert(j);
            }
        }
        if (is_goal_state(task_proxy, current_state)) {
            cout << "FOUND A REDUCTION! Remove marked actions from plan"
                 << "\n";
            Plan improved_plan;
            for (size_t i = 0; i < reduced_plan.size(); i++) {
                if (!marked_actions.contains(i)) {
                    improved_plan.push_back(reduced_plan[i]);
                }
            }
            reduced_plan = improved_plan;
        } else {
            prefix_state = state_registry.get_successor_state(
                prefix_state, prefix_operator);
            i++;
        }
    }
    return reduced_plan;
}