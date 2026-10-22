#include "window_replacement.h"

#include "../component.h"
#include "../evaluator.h"
#include "../operator_cost.h"
#include "../pruning_method.h"
#include "../search_algorithm.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../evaluators/bounded_evaluator.h"
#include "../evaluators/g_evaluator.h"
#include "../evaluators/sum_evaluator.h"
#include "../heuristics/max_heuristic.h"
#include "../open_lists/tiebreaking_open_list.h"
#include "../pruning/null_pruning_method.h"
#include "../search_algorithms/eager_search.h"
#include "../search_algorithms/search_common.h"
#include "../task_utils/task_properties.h"
#include "../tasks/default_value_axioms_task.h"
#include "../tasks/delegating_task.h"
#include "../utils/logging.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

using namespace std;
using namespace task_properties;

// ============================================================
// Temporary task used for one candidate window.
//
// Everything is inherited from the original task except:
//
//     initial state = state before the window
//     goal          = state after the window
//
// The complete original operator set remains available.
// ============================================================

class WindowReplacementTask : public tasks::DelegatingTask {
private:
    vector<int> initial_state_values;
    vector<FactPair> goals;

public:
    WindowReplacementTask(
        const shared_ptr<AbstractTask> &parent, const State &start_state,
        const State &end_state)
        : tasks::DelegatingTask(parent) {
        // ----------------------------------------------------
        // New initial state.
        // ----------------------------------------------------

        start_state.unpack();

        initial_state_values = start_state.get_unpacked_values();

        // ----------------------------------------------------
        // New goal:
        //
        // Require exactly the state that originally occurred
        // immediately after the candidate window.
        // ----------------------------------------------------

        end_state.unpack();

        const vector<int> &end_values = end_state.get_unpacked_values();

        for (int var = 0; var < static_cast<int>(end_values.size()); ++var) {
            goals.emplace_back(var, end_values[var]);
        }
    }

    vector<int> get_initial_state_values() const override {
        return initial_state_values;
    }

    int get_num_goals() const override {
        return static_cast<int>(goals.size());
    }

    FactPair get_goal_fact(int index) const override {
        return goals[index];
    }
};

// ============================================================
// Candidate window.
//
// If the window is:
//
//     o_i, ..., o_j
//
// then:
//
//     start_state = s_i
//     end_state   = s_{j+1}
// ============================================================

struct WindowCandidate {
    size_t start_index;
    size_t end_index;

    StateID start_state;
    StateID end_state;

    int original_cost;

    size_t marked_count;

    size_t length() const {
        return end_index - start_index + 1;
    }
};

// ============================================================
// Compute plan cost.
// ============================================================

static int compute_plan_cost(const Plan &plan, const TaskProxy &task_proxy) {
    OperatorsProxy operators = task_proxy.get_operators();

    int cost = 0;

    for (OperatorID operator_id : plan) {
        cost += operators[operator_id].get_cost();
    }

    return cost;
}

// ============================================================
// Compute all states induced by the plan:
//
//     s_0, s_1, ..., s_n
//
// where:
//
//     pi = <o_0, ..., o_{n-1}>
// ============================================================

static vector<StateID> compute_plan_states(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    OperatorsProxy operators = task_proxy.get_operators();

    vector<StateID> plan_states;

    State current_state = state_registry.get_initial_state();

    plan_states.push_back(current_state.get_id());

    for (OperatorID operator_id : plan) {
        OperatorProxy op = operators[operator_id];

        if (!is_applicable(op, current_state)) {
            cerr << "ERROR: Input plan contains "
                 << "an inapplicable operator." << endl;

            return {};
        }

        current_state = state_registry.get_successor_state(current_state, op);

        plan_states.push_back(current_state.get_id());
    }

    return plan_states;
}

// ============================================================
// Extract candidate windows using Action Elimination.
//
// This follows the Python prototype:
//
// For every operator i:
//
//     mark i
//
//     simulate all following operators
//
//     if an operator becomes inapplicable:
//         mark it
//
//     otherwise:
//         apply it
//
// If at least two operators are marked:
//
//     windowStart = i
//     windowEnd   = last marked operator
//
// The complete interval is then a candidate for replacement.
// ============================================================

static vector<WindowCandidate> extract_action_elimination_windows(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry, const vector<StateID> &plan_states) {
    OperatorsProxy operators = task_proxy.get_operators();

    vector<WindowCandidate> candidates;

    for (size_t i = 0; i < plan.size(); ++i) {
        // ----------------------------------------------------
        // State before operator i.
        // Operator i is considered removed/marked.
        // ----------------------------------------------------

        State current_state = state_registry.lookup_state(plan_states[i]);

        size_t marked_count = 1;

        size_t last_marked = i;

        // ----------------------------------------------------
        // Simulate all following operators.
        // ----------------------------------------------------

        for (size_t j = i + 1; j < plan.size(); ++j) {
            OperatorProxy op = operators[plan[j]];

            if (!is_applicable(op, current_state)) {
                ++marked_count;

                last_marked = j;

                continue;
            }

            current_state =
                state_registry.get_successor_state(current_state, op);
        }

        // Python equivalent:
        //
        //     if len(marked) < 2:
        //         continue
        //
        if (marked_count < 2) {
            continue;
        }

        // ----------------------------------------------------
        // Cost of original interval:
        //
        //     i ... last_marked
        // ----------------------------------------------------

        int window_cost = 0;

        for (size_t j = i; j <= last_marked; ++j) {
            window_cost += operators[plan[j]].get_cost();
        }

        // ----------------------------------------------------
        // Python used:
        //
        //     windowCostLimit = windowCost - 1
        //
        //     if windowCostLimit < 3:
        //         continue
        //
        // Therefore:
        //
        //     original window cost >= 4
        //
        // The Fast Downward search bound below is exclusive,
        // so we do not need to subtract one from the bound.
        // ----------------------------------------------------

        if (window_cost < 4) {
            continue;
        }

        candidates.push_back(
            WindowCandidate{
                i, last_marked,

                // State before first window operator.
                plan_states[i],

                // State after last window operator.
                plan_states[last_marked + 1],

                window_cost,

                marked_count});
    }

    return candidates;
}

// ============================================================
// Search for a cheaper path from the beginning of the window
// to the state after the window.
//
// optional<Plan> is used because an empty plan can itself be a
// valid replacement if start_state == end_state.
// ============================================================

static optional<Plan> find_window_replacement(
    const shared_ptr<AbstractTask> &task, const State &start_state,
    const State &end_state, int original_window_cost) {
    // ========================================================
    // 1. Construct the temporary task.
    // ========================================================

    shared_ptr<AbstractTask> window_task =
        make_shared<WindowReplacementTask>(task, start_state, end_state);

    // ========================================================
    // 2. hmax evaluator.
    //
    // Same construction that already works in
    // OperatorReduction.
    // ========================================================

    shared_ptr<TaskIndependentEvaluator> hmax =
        components::make_auto_task_independent_component<
            max_heuristic::HSPMaxHeuristic, Evaluator>(
            tasks::AxiomHandlingType::APPROXIMATE_NEGATIVE, true, "hmax",
            utils::Verbosity::NORMAL);

    // f = g + hmax

    shared_ptr<TaskIndependentEvaluator> g =
        components::make_auto_task_independent_component<
            g_evaluator::GEvaluator, Evaluator>(
            "window.g_eval", utils::Verbosity::NORMAL);

    shared_ptr<TaskIndependentEvaluator> f =
        components::make_auto_task_independent_component<
            sum_evaluator::SumEvaluator, Evaluator>(
            vector<shared_ptr<TaskIndependentEvaluator>>({g, hmax}),
            "window.f_eval", utils::Verbosity::NORMAL);

    // bounded_f = infinity if f >= original window cost

    shared_ptr<TaskIndependentEvaluator> bounded_f =
        components::make_auto_task_independent_component<
            bounded_evaluator::BoundedEvaluator, Evaluator>(
            f, original_window_cost, "window.bounded_f_eval",
            utils::Verbosity::NORMAL);

    // A*-style tie breaking: bounded f first, hmax second.

    vector<shared_ptr<TaskIndependentEvaluator>> open_list_evaluators = {
        bounded_f, hmax};

    shared_ptr<TaskIndependentOpenListFactory> open_list =
        components::make_auto_task_independent_component<
            tiebreaking_open_list::TieBreakingOpenListFactory, OpenListFactory>(
            open_list_evaluators, false, false);

    vector<shared_ptr<TaskIndependentEvaluator>> preferred;

    shared_ptr<TaskIndependentEvaluator> lazy_evaluator = nullptr;

    shared_ptr<TaskIndependentPruningMethod> pruning_method =
        components::make_auto_task_independent_component<
            null_pruning_method::NullPruningMethod, PruningMethod>(
            utils::Verbosity::NORMAL);

    shared_ptr<TaskIndependentSearchAlgorithm> astar =
        components::make_auto_task_independent_component<
            eager_search::EagerSearch, SearchAlgorithm>(
            open_list, true, f, preferred, pruning_method, lazy_evaluator,
            OperatorCost::NORMAL, original_window_cost,
            numeric_limits<double>::infinity(),
            "window replacement A* with bounded f", utils::Verbosity::NORMAL);

    // ========================================================
    // 5. Bind search to the temporary task.
    //
    // This creates a normal Fast Downward search, but with:
    //
    //     initial state = window start state
    //     goal          = window end state
    // ========================================================

    shared_ptr<SearchAlgorithm> search = astar->bind_task(window_task);

    search->search();

    if (!search->found_solution()) {
        return nullopt;
    }

    return search->get_plan();
}

// ============================================================
// Main improvement method.
// ============================================================

Plan ActionEliminationWindowReplacement::improve(
    const Plan &plan, const shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    cout << "Starting Action Elimination "
         << "window replacement" << endl;

    TaskProxy task_proxy(*task);

    int original_plan_cost = compute_plan_cost(plan, task_proxy);

    // ========================================================
    // 1. Compute states of the input plan.
    // ========================================================

    vector<StateID> plan_states =
        compute_plan_states(plan, task_proxy, state_registry);

    if (plan_states.empty()) {
        cout << "Could not compute plan states. "
             << "Keeping original plan." << endl;

        return plan;
    }

    // ========================================================
    // 2. Extract Action-Elimination candidate windows.
    // ========================================================

    vector<WindowCandidate> candidates = extract_action_elimination_windows(
        plan, task_proxy, state_registry, plan_states);

    cout << "Window candidates: " << candidates.size() << endl;

    // ========================================================
    // Sort candidate windows.
    //
    // Your Python implementation effectively prioritizes
    // shorter windows once the cost filter has been applied.
    //
    // For equal lengths, try higher-cost windows first.
    // ========================================================

    sort(
        candidates.begin(), candidates.end(),
        [](const WindowCandidate &left, const WindowCandidate &right) {
            if (left.length() != right.length()) {
                return left.length() < right.length();
            }

            return left.original_cost > right.original_cost;
        });

    size_t attempted_windows = 0;

    // ========================================================
    // 3. Try replacing candidate windows.
    // ========================================================

    for (const WindowCandidate &candidate : candidates) {
        // Python:
        //
        //     if windowLength > 15:
        //         continue
        //
        if (candidate.length() > 15) {
            continue;
        }

        ++attempted_windows;

        cout << "Trying window [" << candidate.start_index << ", "
             << candidate.end_index << "]"
             << " length=" << candidate.length()
             << " cost=" << candidate.original_cost
             << " marked=" << candidate.marked_count << endl;

        State start_state = state_registry.lookup_state(candidate.start_state);

        State end_state = state_registry.lookup_state(candidate.end_state);

        optional<Plan> replacement = find_window_replacement(
            task, start_state, end_state, candidate.original_cost);

        if (!replacement.has_value()) {
            continue;
        }

        // ====================================================
        // Construct:
        //
        //     original prefix
        //     + replacement
        //     + original suffix
        //
        // Original window:
        //
        //     [start_index, end_index]
        //
        // is completely removed.
        // ====================================================

        Plan candidate_plan;

        // ----------------------------------------------------
        // Prefix:
        //
        //     [0, start_index)
        // ----------------------------------------------------

        candidate_plan.insert(
            candidate_plan.end(), plan.begin(),
            plan.begin() + candidate.start_index);

        // ----------------------------------------------------
        // Replacement.
        // ----------------------------------------------------

        candidate_plan.insert(
            candidate_plan.end(), replacement->begin(), replacement->end());

        // ----------------------------------------------------
        // Suffix:
        //
        //     (end_index, plan.size())
        // ----------------------------------------------------

        candidate_plan.insert(
            candidate_plan.end(), plan.begin() + candidate.end_index + 1,
            plan.end());

        // ====================================================
        // Defensive cost check.
        //
        // The bounded window search should already guarantee
        // an improvement, but checking the complete plan here
        // makes the method robust against implementation errors.
        // ====================================================

        int candidate_cost = compute_plan_cost(candidate_plan, task_proxy);

        if (candidate_cost < original_plan_cost) {
            cout << "Window replacement improved plan: " << original_plan_cost
                 << " -> " << candidate_cost << endl;

            cout << "Window searches attempted: " << attempted_windows << endl;

            return candidate_plan;
        }
    }

    // ========================================================
    // Nothing found.
    // ========================================================

    cout << "No improving window replacement found." << endl;

    cout << "Window searches attempted: " << attempted_windows << endl;

    return plan;
}