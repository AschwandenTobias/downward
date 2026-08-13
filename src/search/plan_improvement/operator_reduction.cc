#include "operator_reduction.h"

#include "operator_pruner.h"

#include "../component.h"
#include "../evaluator.h"
#include "../operator_cost.h"
#include "../pruning_method.h"
#include "../search_algorithm.h"
#include "../state_registry.h"

#include "../heuristics/lm_cut_heuristic.h"
#include "../heuristics/lm_cut_landmarks.h"
#include "../search_algorithms/eager_search.h"
#include "../search_algorithms/search_common.h"
#include "../utils/logging.h"

#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace std;

OperatorReduction::OperatorReduction(OperatorReductionType reduction_type)
    : reduction_type(reduction_type) {
}

Plan OperatorReduction::improve(
    const Plan &plan, const shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    (void)state_registry;

    cout << "Starting operator reduction" << endl;
    cout << "Initial plan length: " << plan.size() << endl;

    /*
     * ------------------------------------------------------------
     * 1. Select pruning strategy.
     * ------------------------------------------------------------
     */
    shared_ptr<components::TaskIndependentComponent<PruningMethod>>
        operator_pruner;

    if (reduction_type == OperatorReductionType::IDS) {
        cout << "Using exact OperatorID reduction" << endl;

        operator_pruner = make_shared<TaskIndependentOperatorPruner>(
            utils::Verbosity::NORMAL, plan);
    } else {
        cout << "Using operator-name reduction" << endl;

        operator_pruner = make_shared<TaskIndependentOperatorNamePruner>(
            utils::Verbosity::NORMAL, plan);
    }

    /*
     * ------------------------------------------------------------
     * 2. Create LM-cut.
     * ------------------------------------------------------------
     */
    auto lmcut = components::make_auto_task_independent_component<
        lm_cut_heuristic::LandmarkCutHeuristic, Evaluator>(
        true, true, true, "lmcut", utils::Verbosity::NORMAL);

    /*
     * ------------------------------------------------------------
     * 3. Build standard A* components.
     * ------------------------------------------------------------
     */
    auto astar_components =
        search_common::create_astar_open_list_factory_and_f_eval(
            lmcut, utils::Verbosity::NORMAL);

    vector<shared_ptr<components::TaskIndependentComponent<Evaluator>>>
        preferred;

    shared_ptr<components::TaskIndependentComponent<Evaluator>> lazy_evaluator =
        nullptr;

    /*
     * ------------------------------------------------------------
     * 4. Construct A* as EagerSearch.
     * ------------------------------------------------------------
     */
    auto astar = components::make_auto_task_independent_component<
        eager_search::EagerSearch, SearchAlgorithm>(
        astar_components.first, true, astar_components.second, preferred,
        operator_pruner, lazy_evaluator, OperatorCost::NORMAL,
        numeric_limits<int>::max(), numeric_limits<double>::infinity(),
        "operator reduction A*", utils::Verbosity::NORMAL);

    /*
     * ------------------------------------------------------------
     * 5. Bind task and run search.
     * ------------------------------------------------------------
     */
    shared_ptr<SearchAlgorithm> search = astar->bind_task(task);

    cout << "Starting restricted A* search" << endl;

    search->search();

    /*
     * ------------------------------------------------------------
     * 6. Return improved plan.
     * ------------------------------------------------------------
     */
    if (search->found_solution()) {
        Plan improved_plan = search->get_plan();

        cout << "Restricted A* found a solution" << endl;
        cout << "Old plan length: " << plan.size() << endl;
        cout << "New plan length: " << improved_plan.size() << endl;

        return improved_plan;
    }

    cout << "Restricted A* found no solution. "
         << "Keeping original plan." << endl;

    return plan;
}