#include "reconnection_reduction.h"

#include "../operator_id.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../plugins/plugin.h"
#include "../task_utils/task_properties.h"

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <map>

using namespace std;
using namespace task_properties;

struct GreedyStateInfo {
    State state;
    size_t index;
    size_t prefix_cost;
};

static vector<int> get_state_key(const State &state) {
    state.unpack();
    return state.get_unpacked_values();
}

static Plan remove_loops_from_plan(
    const Plan &plan, const vector<GreedyStateInfo> &plan_states) {
    Plan loop_free_plan;

    vector<vector<int>> state_ids;
    map<vector<int>, size_t> state_position;

    vector<int> initial_state_id = get_state_key(plan_states[0].state);

    state_ids.push_back(initial_state_id);
    state_position[initial_state_id] = 0;

    for (size_t i = 0; i < plan.size(); ++i) {
        vector<int> next_state_id = get_state_key(plan_states[i + 1].state);

        if (state_position.count(next_state_id) == 0) {
            loop_free_plan.push_back(plan[i]);

            state_ids.push_back(next_state_id);

            state_position[next_state_id] = state_ids.size() - 1;
        } else {
            size_t old_position = state_position[next_state_id];

            for (size_t k = old_position + 1; k < state_ids.size(); ++k) {
                state_position.erase(state_ids[k]);
            }

            state_ids.resize(old_position + 1);

            while (loop_free_plan.size() > old_position) {
                loop_free_plan.pop_back();
            }
        }
    }

    return loop_free_plan;
}

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

    plan_states.push_back({current_state, plan.size(), prefix_cost});

    return plan_states;
}

static map<vector<int>, vector<size_t>> build_state_position_lookup(
    const vector<GreedyStateInfo> &plan_states) {

    map<vector<int>, vector<size_t>> state_positions;

    for (size_t k = 0; k < plan_states.size(); ++k) {
        vector<int> state_key = get_state_key(plan_states[k].state);

        state_positions[state_key].push_back(k);
    }

    return state_positions;
}

namespace reconnection_reduction {
Plan ReconnectionReduction::improve(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    cout << "\n"
         << "!!!!! Starting reconnection reduction !!!!!"
         << "\n"
         << endl;
    OperatorsProxy operators = task_proxy.get_operators();

    Plan current_plan = plan;
    size_t current_plan_size = current_plan.size();
    vector<GreedyStateInfo> plan_states =
        extract_state_info_from_plan(current_plan, task_proxy, state_registry);
    current_plan = remove_loops_from_plan(current_plan, plan_states);
    if (current_plan_size > current_plan.size()) {
        plan_states = extract_state_info_from_plan(
            current_plan, task_proxy, state_registry);
    }
    map<vector<int>, vector<size_t>> state_positions = build_state_position_lookup(plan_states);
    size_t i = 0;

    while (i < current_plan.size()) {
        reduction_attempts++;
        State simulated_state = plan_states[i].state;

        Plan candidate_segment;

        size_t candidate_cost = 0;

        bool best_reduction_found = false;

        Plan best_plan;

        size_t current_plan_cost = plan_states.back().prefix_cost;

        size_t best_plan_cost = current_plan_cost;

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

            vector<int> simulated_state_key = get_state_key(simulated_state);

            auto it = state_positions.find(simulated_state_key);

            if (it == state_positions.end()) {
                continue;
            }

            size_t k = it->second[0];

            size_t prefix_cost = plan_states[i].prefix_cost;

            size_t suffix_cost = current_plan_cost - plan_states[k].prefix_cost;

            size_t complete_candidate_cost =
                prefix_cost + candidate_cost + suffix_cost;

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
        if (best_reduction_found) {
            current_plan = std::move(best_plan);

            ++successfull_reductions;
            plan_states = extract_state_info_from_plan(
                current_plan, task_proxy, state_registry);

            state_positions = build_state_position_lookup(plan_states);
            continue;
        }
        ++i;
    }

    cout << "\n"
         << "!!!!! Finished reconnection reduction !!!!!"
         << "\n"
         << endl;

    return current_plan;
}
void ReconnectionReduction::print_statistics() const {
    utils::g_log << "Reconnection Reduction statistics:" << endl;
    utils::g_log << "Reduction attempts: " << reduction_attempts << endl;
    utils::g_log << "Successful reductions: " << successfull_reductions << endl;
}

class ReconnectionReductionFeature : public plugins::TypedFeature<PlanImprovement> {
public:
    ReconnectionReductionFeature() : TypedFeature("reconnection_reduction") {
        document_title("Reconnection Reduction");
        document_synopsis("Improves plans by removing unnecessary actions with reconnecting to states of the original plan.");
    }
    std::shared_ptr<PlanImprovement> create_component(
        const plugins::Options &) const override {
        return std::make_shared<ReconnectionReduction>();
    }
};

static plugins::FeaturePlugin<ReconnectionReductionFeature> _plugin;
}