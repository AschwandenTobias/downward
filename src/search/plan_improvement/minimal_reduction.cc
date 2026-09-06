#include "minimal_reduction.h"

#include "../evaluator.h"
#include "../open_list.h"
#include "../search_algorithm.h"

#include "../heuristics/max_heuristic.h"
#include "../pruning/null_pruning_method.h"
#include "../search_algorithms/eager_search.h"
#include "../search_algorithms/search_common.h"
#include "../tasks/default_value_axioms_task.h"
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

    shared_ptr<TaskIndependentEvaluator> hmax =
        components::make_auto_task_independent_component<
            max_heuristic::HSPMaxHeuristic, Evaluator>(
            tasks::AxiomHandlingType::APPROXIMATE_NEGATIVE, true, "hmax",
            utils::Verbosity::NORMAL);

    pair<
        shared_ptr<TaskIndependentOpenListFactory>,
        shared_ptr<TaskIndependentEvaluator>>
        astar_components =
            search_common::create_astar_open_list_factory_and_f_eval(
                hmax, utils::Verbosity::NORMAL);

    vector<shared_ptr<TaskIndependentEvaluator>> preferred_evaluators;

    shared_ptr<TaskIndependentEvaluator> lazy_evaluator = nullptr;

    shared_ptr<TaskIndependentPruningMethod> pruning_method =
        components::make_auto_task_independent_component<
            null_pruning_method::NullPruningMethod, PruningMethod>(
            utils::Verbosity::NORMAL);

    shared_ptr<TaskIndependentSearchAlgorithm> astar =
        components::make_auto_task_independent_component<
            eager_search::EagerSearch, SearchAlgorithm>(
            astar_components.first, true, astar_components.second,
            preferred_evaluators, pruning_method, lazy_evaluator,
            OperatorCost::NORMAL, numeric_limits<int>::max(),
            numeric_limits<double>::infinity(),
            "minimal reduction A* with hmax", utils::Verbosity::NORMAL);

    shared_ptr<SearchAlgorithm> search = astar->bind_task(minimal_task);

    search->search();

    if (!search->found_solution()) {
        return plan;
    }

    const Plan &transformed_plan = search->get_plan();

    int transformed_cost = 0;

    for (OperatorID transformed_id : transformed_plan) {
        int index = transformed_id.get_index();

        int op_cost = minimal_task->get_operator_cost(index, false);

        cout << "Transformed operator " << index << ": "
             << minimal_task->get_operator_name(index, false)
             << " cost=" << op_cost << endl;

        transformed_cost += op_cost;
    }

    Plan reduced_plan;

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