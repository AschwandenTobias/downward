#include "greedy_plan_states.h"

#include "../operator_id.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../task_utils/task_properties.h"

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace task_properties;

struct GreedyStateInfo {
    State state;
    size_t index;
    size_t prefix_cost;
};

/*
 * Extract every state occurring along the plan.
 *
 * plan_states[i] is the state immediately BEFORE
 * executing plan[i].
 *
 * The last entry is the state after executing
 * the complete plan.
 */
static vector<GreedyStateInfo> extract_state_info_from_plan(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    OperatorsProxy operators = task_proxy.get_operators();

    State current_state = state_registry.get_initial_state();

    vector<GreedyStateInfo> plan_states;

    size_t prefix_cost = 0;

    for (size_t i = 0; i < plan.size(); ++i) {
        plan_states.push_back({current_state, i, prefix_cost});

        OperatorProxy op = operators[plan[i]];

        prefix_cost += static_cast<size_t>(op.get_cost());

        current_state = state_registry.get_successor_state(current_state, op);
    }

    /*
     * Final state after the complete plan.
     */
    plan_states.push_back({current_state, plan.size(), prefix_cost});

    return plan_states;
}

/*
 * StateID -> all positions at which this exact state
 * occurs on the current plan.
 */
static unordered_map<int, vector<size_t>> build_state_position_lookup(
    const vector<GreedyStateInfo> &plan_states) {
    unordered_map<int, vector<size_t>> state_positions;

    for (size_t k = 0; k < plan_states.size(); ++k) {
        int state_id = plan_states[k].state.get_id().get_value();

        state_positions[state_id].push_back(k);
    }

    return state_positions;
}

Plan GreedyPlanStates::improve(
    const Plan &plan, const shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    cout << "\n"
         << "!!!!! Starting greedy plan states !!!!!"
         << "\n"
         << endl;

    TaskProxy task_proxy(*task);

    OperatorsProxy operators = task_proxy.get_operators();

    Plan current_plan = plan;

    /*
     * Compute all states of the current plan.
     */
    vector<GreedyStateInfo> plan_states =
        extract_state_info_from_plan(current_plan, task_proxy, state_registry);

    /*
     * Fast lookup for exact state reconnections.
     */
    unordered_map<int, vector<size_t>> state_positions =
        build_state_position_lookup(plan_states);

    size_t i = 0;

    size_t number_of_reductions = 0;

    while (i < current_plan.size()) {
        /*
         * Try removing action i.
         *
         * Simulation starts in the state immediately
         * before action i.
         */
        State simulated_state = plan_states[i].state;

        Plan candidate_segment;

        size_t candidate_cost = 0;

        bool reduction_applied = false;

        /*
         * Skip action i and consider the remaining
         * actions in their original order.
         *
         * Whenever an action is applicable in the
         * simulated state, execute it.
         */
        for (size_t j = i + 1; j < current_plan.size(); ++j) {
            OperatorID op_id = current_plan[j];

            OperatorProxy op = operators[op_id];

            /*
             * Inapplicable actions are simply skipped.
             */
            if (!is_applicable(op, simulated_state)) {
                continue;
            }

            simulated_state =
                state_registry.get_successor_state(simulated_state, op);

            candidate_segment.push_back(op_id);

            candidate_cost += static_cast<size_t>(op.get_cost());

            /*
             * Check whether this exact simulated state
             * occurs later on the current plan.
             */
            int simulated_state_id = simulated_state.get_id().get_value();

            unordered_map<int, vector<size_t>>::const_iterator state_it =
                state_positions.find(simulated_state_id);

            if (state_it == state_positions.end()) {
                continue;
            }

            const vector<size_t> &matching_positions = state_it->second;

            /*
             * We require k > j.
             *
             * This means we reconnect to a state that
             * lies strictly after the position of the
             * action we just considered.
             */
            vector<size_t>::const_iterator k_it = upper_bound(
                matching_positions.begin(), matching_positions.end(), j);

            for (; k_it != matching_positions.end(); ++k_it) {
                size_t k = *k_it;

                /*
                 * Cost of the part of the current plan
                 * that would be replaced:
                 *
                 *     [i, k)
                 */
                size_t old_segment_cost =
                    plan_states[k].prefix_cost - plan_states[i].prefix_cost;

                /*
                 * Only accept an actual improvement.
                 */
                if (candidate_cost >= old_segment_cost) {
                    continue;
                }

                /*
                 * We found the FIRST improving
                 * reconnection for position i.
                 *
                 * Construct:
                 *
                 * prefix [0, i)
                 * +
                 * candidate_segment
                 * +
                 * suffix [k, end)
                 */
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

                current_plan = std::move(improved_plan);

                ++number_of_reductions;

                /*
                 * The plan has changed.
                 *
                 * Therefore all states, prefix costs,
                 * and state positions must be rebuilt.
                 */
                plan_states = extract_state_info_from_plan(
                    current_plan, task_proxy, state_registry);

                state_positions = build_state_position_lookup(plan_states);

                reduction_applied = true;

                break;
            }

            if (reduction_applied) {
                break;
            }
        }

        /*
         * Important:
         *
         * After a successful reduction, retry the SAME i.
         *
         * The action that was previously later in the
         * plan may now have shifted into position i and
         * another reduction may be possible.
         */
        if (reduction_applied) {
            continue;
        }

        /*
         * Nothing could be improved at position i.
         * Move to the next position.
         */
        ++i;
    }

    cout << "Greedy plan-state reductions: " << number_of_reductions << endl;

    cout << "Original plan length: " << plan.size() << endl;

    cout << "Greedy plan-state plan length: " << current_plan.size() << endl;

    cout << "\n"
         << "!!!!! Finished greedy plan states !!!!!"
         << "\n"
         << endl;

    return current_plan;
}