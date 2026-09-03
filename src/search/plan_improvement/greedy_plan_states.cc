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

    vector<GreedyStateInfo> plan_states =
        extract_state_info_from_plan(current_plan, task_proxy, state_registry);

    unordered_map<int, vector<size_t>> state_positions =
        build_state_position_lookup(plan_states);

    size_t i = 0;
    size_t number_of_reductions = 0;

    while (i < current_plan.size()) {
        /*
         * Try removing current_plan[i].
         *
         * Simulation starts immediately before action i.
         */
        State simulated_state = plan_states[i].state;

        Plan candidate_segment;

        size_t candidate_cost = 0;

        /*
         * Best complete replacement found for position i.
         */
        bool best_reduction_found = false;

        Plan best_plan;

        /*
         * Cost of the current complete plan.
         */
        size_t current_plan_cost = plan_states.back().prefix_cost;

        size_t best_plan_cost = current_plan_cost;

        /*
         * Skip action i and greedily simulate all later
         * applicable actions.
         */
        for (size_t j = i + 1; j < current_plan.size(); ++j) {
            OperatorID op_id = current_plan[j];

            OperatorProxy op = operators[op_id];

            if (!is_applicable(op, simulated_state)) {
                continue;
            }

            simulated_state =
                state_registry.get_successor_state(simulated_state, op);

            candidate_segment.push_back(op_id);

            candidate_cost += static_cast<size_t>(op.get_cost());

            /*
             * ------------------------------------------------
             * Plan-state reconnections
             * ------------------------------------------------
             *
             * Check whether the current simulated state is
             * exactly one of the later states of the current
             * plan.
             */
            int simulated_state_id = simulated_state.get_id().get_value();

            unordered_map<int, vector<size_t>>::const_iterator state_it =
                state_positions.find(simulated_state_id);

            if (state_it != state_positions.end()) {
                const vector<size_t> &matching_positions = state_it->second;

                /*
                 * Only reconnect to states occurring after
                 * the action position j.
                 */
                vector<size_t>::const_iterator k_it = upper_bound(
                    matching_positions.begin(), matching_positions.end(), j);

                for (; k_it != matching_positions.end(); ++k_it) {
                    size_t k = *k_it;

                    /*
                     * Complete cost of:
                     *
                     * prefix [0, i)
                     * +
                     * candidate_segment
                     * +
                     * suffix [k, end)
                     */
                    size_t prefix_cost = plan_states[i].prefix_cost;

                    size_t suffix_cost =
                        current_plan_cost - plan_states[k].prefix_cost;

                    size_t complete_candidate_cost =
                        prefix_cost + candidate_cost + suffix_cost;

                    /*
                     * Only remember an actual improvement,
                     * and only if it is the best one found
                     * so far for this i.
                     */
                    if (complete_candidate_cost >= best_plan_cost) {
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

                    best_plan = std::move(improved_plan);

                    best_plan_cost = complete_candidate_cost;

                    best_reduction_found = true;
                }
            }
        }

        /*
         * ----------------------------------------------------
         * AE-style goal reduction
         * ----------------------------------------------------
         *
         * We deliberately continued the complete simulation
         * even if an earlier plan-state reconnection existed.
         *
         * If the resulting simulated state is already a goal,
         * then:
         *
         * prefix [0, i)
         * +
         * candidate_segment
         *
         * is itself a complete valid plan.
         */
        if (is_goal_state(task_proxy, simulated_state)) {
            size_t goal_candidate_cost =
                plan_states[i].prefix_cost + candidate_cost;

            if (goal_candidate_cost < best_plan_cost) {
                Plan goal_plan;

                goal_plan.insert(
                    goal_plan.end(), current_plan.begin(),
                    current_plan.begin() + i);

                goal_plan.insert(
                    goal_plan.end(), candidate_segment.begin(),
                    candidate_segment.end());

                best_plan = std::move(goal_plan);

                best_plan_cost = goal_candidate_cost;

                best_reduction_found = true;
            }
        }

        /*
         * ----------------------------------------------------
         * Apply the best reduction found for this position.
         * ----------------------------------------------------
         */
        if (best_reduction_found) {
            current_plan = std::move(best_plan);

            ++number_of_reductions;

            /*
             * The plan changed, so all plan-state information
             * is outdated.
             */
            plan_states = extract_state_info_from_plan(
                current_plan, task_proxy, state_registry);

            state_positions = build_state_position_lookup(plan_states);

            /*
             * Retry the same position.
             */
            continue;
        }

        /*
         * No useful reduction found for i.
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