#include "action_elimination_plan_states.h"

#include "action_elimination.h"

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

vector<ReductionCandidate> ActionEliminationPlanStates::candidate_extractor(
    const Plan &plan, const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry, bool apply_reductions) {
    vector<ReductionCandidate> reduction_candidates;

    TaskProxy task_proxy(*task);
    OperatorsProxy operators = task_proxy.get_operators();

    Plan current_plan = plan;

    // Get all the states with prefix cost from the original plan.
    vector<StateInfo> plan_states =
        extract_state_info_from_plan(current_plan, task_proxy, state_registry);

    size_t i = 0;

    while (i < current_plan.size()) {
        State simulated_state = plan_states[i].state;
        // Contains the applicable actions.
        Plan candidate_segment;
        size_t candidate_cost = 0;

        bool reduction_applied = false;

        for (size_t j = i + 1; j < current_plan.size(); ++j) {
            OperatorProxy op = operators[current_plan[j]];

            if (!is_applicable(op, simulated_state)) {
                continue;
            }

            simulated_state =
                state_registry.get_successor_state(simulated_state, op);

            candidate_segment.push_back(current_plan[j]);

            candidate_cost += op.get_cost();

            for (size_t k = j + 1; k < plan_states.size(); ++k) {
                if (simulated_state != plan_states[k].state) {
                    continue;
                }
                size_t old_segment_cost =
                    plan_states[k].prefix_cost - plan_states[i].prefix_cost;

                if (candidate_cost >= old_segment_cost) {
                    continue;
                }

                ReductionCandidate candidate{
                    i,
                    k,
                    plan_states[i].state.get_id(),
                    plan_states[k].state.get_id(),
                    candidate_segment,
                    candidate_cost};

                reduction_candidates.push_back(candidate);

                // If we are in the run that does not greedily applies
                // reductions, continue, otherwise apply the reduction.
                if (!apply_reductions) {
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

                current_plan = std::move(improved_plan);

                // Since stuff has changed, we have to recalculate the prefix
                // costs etc.
                plan_states = extract_state_info_from_plan(
                    current_plan, task_proxy, state_registry);

                reduction_applied = true;
                break;
            }
            if (reduction_applied) {
                break;
            }
        }
        if (reduction_applied) {
            continue;
        } else {
            // Only here increment i, since we otherwise skip some improvements
            ++i;
        }
    }
    return reduction_candidates;
}

Plan ActionEliminationPlanStates::improve(
    const Plan &plan, const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    TaskProxy task_proxy(*task);
    OperatorsProxy operators = task_proxy.get_operators();
    cout << "\n\n\n!!!!! I am at the start of ae_plan_states !!!!\n" << endl;
    vector<ReductionCandidate> reduction_candidates =
        candidate_extractor(plan, task, state_registry, false);
    cout << "Number of reduction candidates: " << reduction_candidates.size()
         << "\n\n"
         << endl;
    vector<ReductionCandidate> greedy_candidates =
        candidate_extractor(plan, task, state_registry, true);

    reduction_candidates.insert(
        reduction_candidates.end(), greedy_candidates.begin(),
        greedy_candidates.end());

    cout << "Number of reduction candidates after the greedy: "
         << reduction_candidates.size() << "\n\n"
         << endl;

    return plan;
}