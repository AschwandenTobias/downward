#include "minimal_reduction.h"

#include "../evaluator.h"
#include "../open_list.h"
#include "../search_algorithm.h"

#include "../heuristics/blind_search_heuristic.h"
#include "../pruning/null_pruning_method.h"
#include "../search_algorithms/eager_search.h"
#include "../search_algorithms/search_common.h"
#include "../tasks/minimal_reduction_task.h"
#include "../utils/logging.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

using namespace std;

Plan MinimalReduction::improve(
    const Plan &plan, const shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    (void)state_registry;
    cout << "\nAt the start of minimal_reduction" << endl;
    cout << "Initial plan length: " << plan.size() << endl;
    shared_ptr<AbstractTask> minimal_task =
        make_shared<MinimalReductionTask>(task, plan);
    // cout << "\nSucessfully? make a minimal_task" << endl;
    shared_ptr<TaskIndependentEvaluator> blind =
        components::make_auto_task_independent_component<
            blind_search_heuristic::BlindSearchHeuristic, Evaluator>(
            true, "blind", utils::Verbosity::NORMAL);
    // cout << "Made a taskIndependantEvaluator" << endl;
    pair<
        shared_ptr<TaskIndependentOpenListFactory>,
        shared_ptr<TaskIndependentEvaluator>>
        astar_components =
            search_common::create_astar_open_list_factory_and_f_eval(
                blind, utils::Verbosity::NORMAL);
    // cout << "Made a pair of openListactors and Evaluator" << endl;

    vector<shared_ptr<TaskIndependentEvaluator>> preferred_evaluators;
    // cout << "Got a vector preferred_evaluators" << endl;

    shared_ptr<TaskIndependentEvaluator> lazy_evaluator = nullptr;

    shared_ptr<TaskIndependentPruningMethod> pruning_method =
        components::make_auto_task_independent_component<
            null_pruning_method::NullPruningMethod, PruningMethod>(
            utils::Verbosity::NORMAL);
    // cout << "Building astar" << endl;
    shared_ptr<TaskIndependentSearchAlgorithm> astar =
        components::make_auto_task_independent_component<
            eager_search::EagerSearch, SearchAlgorithm>(
            astar_components.first, true, astar_components.second,
            preferred_evaluators, pruning_method, lazy_evaluator,
            OperatorCost::NORMAL, numeric_limits<int>::max(),
            numeric_limits<double>::infinity(), "minimal reduction A*",
            utils::Verbosity::NORMAL);
    // cout << "Right before binding the minimal task to astar" << endl;

    shared_ptr<SearchAlgorithm> search = astar->bind_task(minimal_task);
    // cout << "Starting the search with the minimal_task" << endl;
    search->search();

    if (!search->found_solution()) {
        return plan;
    }
    const Plan &transformed_plan = search->get_plan();
    // Added the cost here for debugging.
    // cout << "Transformed plan length: " << transformed_plan.size() << endl;

    int transformed_cost = 0;

    for (OperatorID transformed_id : transformed_plan) {
        int index = transformed_id.get_index();

        int op_cost = minimal_task->get_operator_cost(index, false);

        cout << "Transformed operator " << index << ": "
             << minimal_task->get_operator_name(index, false)
             << " cost=" << op_cost << endl;

        transformed_cost += op_cost;
    }

    // cout << "Transformed plan cost: " << transformed_cost << endl;

    Plan reduced_plan;
    // Also for debugging.
    TaskProxy original_task_proxy(*task);
    OperatorsProxy original_operators = original_task_proxy.get_operators();

    int decoded_cost = 0;

    for (OperatorID transformed_id : transformed_plan) {
        int index = transformed_id.get_index();

        if (index % 2 == 0) {
            int plan_position = index / 2;

            OperatorID original_id = plan[plan_position];

            reduced_plan.push_back(original_id);

            decoded_cost += original_operators[original_id].get_cost();
        }
    }

    cout << "Decoded plan length: " << reduced_plan.size() << endl;

    cout << "Decoded plan cost: " << decoded_cost << endl;

    return reduced_plan;
}