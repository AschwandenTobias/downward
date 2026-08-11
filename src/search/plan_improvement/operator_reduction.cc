#include "operator_reduction.h"

#include "operator_pruner.h"

#include "../component.h"
#include "../evaluator.h"
#include "../operator_cost.h"
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

Plan OperatorReduction::improve(
    const Plan &plan, const shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    // The second search creates its own StateRegistry.
    (void)state_registry;
    cout << "\n";
    cout << "\n";
    cout << "\n";
    cout << "\n";
    cout << "!!!!! Starting operator reduction !!!!!" << endl;
    cout << "Initial plan length: " << plan.size() << endl;

    /*
     * ------------------------------------------------------------
     * 1. Create our task-independent pruning component.
     * ------------------------------------------------------------
     *
     * IMPORTANT:
     *
     * Do NOT use:
     *
     * components::make_auto_task_independent_component<
     *     OperatorPruner, PruningMethod>(..., plan);
     *
     * Fast Downward's automatic binder cannot handle
     * vector<OperatorID>.
     *
     * TaskIndependentOperatorPruner stores the Plan itself and
     * creates OperatorPruner once a task is bound.
     */
    auto operator_pruner = make_shared<TaskIndependentOperatorPruner>(
        utils::Verbosity::NORMAL, plan);

    /*
     * ------------------------------------------------------------
     * 2. Create task-independent LM-cut.
     * ------------------------------------------------------------
     *
     * Actual constructor after task binding:
     *
     * LandmarkCutHeuristic(
     *     task,
     *     use_goal_zone_detection,
     *     use_border_detection,
     *     cache_estimates,
     *     description,
     *     verbosity)
     *
     * The first argument, task, is inserted by bind_task().
     */
    auto lmcut = components::make_auto_task_independent_component<
        lm_cut_heuristic::LandmarkCutHeuristic,
        Evaluator>(
        true, // goal_zone_detection
        true, // border_detection
        true, // cache_estimates
        "lmcut", utils::Verbosity::NORMAL);

    /*
     * ------------------------------------------------------------
     * 3. Create standard A* open list and f = g + h evaluator.
     * ------------------------------------------------------------
     *
     * This is exactly the helper used by plugin_astar.cc.
     */
    auto astar_components =
        search_common::create_astar_open_list_factory_and_f_eval(
            lmcut, utils::Verbosity::NORMAL);

    /*
     * A* normally has no preferred-operator evaluators.
     */
    vector<shared_ptr<components::TaskIndependentComponent<Evaluator>>>
        preferred;

    /*
     * No lazy evaluator.
     */
    shared_ptr<components::TaskIndependentComponent<Evaluator>> lazy_evaluator =
        nullptr;

    /*
     * ------------------------------------------------------------
     * 4. Construct task-independent EagerSearch.
     * ------------------------------------------------------------
     *
     * This corresponds to astar(lmcut()), except that our custom
     * OperatorPruner is supplied as the pruning method.
     */
    auto astar = components::make_auto_task_independent_component<
        eager_search::EagerSearch, SearchAlgorithm>(
        astar_components.first,
        true, // reopen_closed
        astar_components.second, preferred, operator_pruner, lazy_evaluator,
        OperatorCost::NORMAL, numeric_limits<int>::max(),
        numeric_limits<double>::infinity(), "operator reduction A*",
        utils::Verbosity::NORMAL);

    /*
     * ------------------------------------------------------------
     * 5. Bind everything to the actual task.
     * ------------------------------------------------------------
     *
     * This recursively creates the real:
     *
     *   EagerSearch(task, ...)
     *   LandmarkCutHeuristic(task, ...)
     *   OperatorPruner(task, ...)
     */
    shared_ptr<SearchAlgorithm> search = astar->bind_task(task);

    /*
     * ------------------------------------------------------------
     * 6. Run restricted A*.
     * ------------------------------------------------------------
     */
    cout << "Starting restricted A* search" << endl;

    search->search();

    /*
     * ------------------------------------------------------------
     * 7. Return resulting plan.
     * ------------------------------------------------------------
     */
    if (search->found_solution()) {
        Plan improved_plan = search->get_plan();

        cout << "Restricted A* found a solution" << endl;
        cout << "Old plan length: " << plan.size() << endl;
        cout << "New plan length: " << improved_plan.size() << endl;

        return improved_plan;
    }

    /*
     * Normally this should not happen, because every operator used
     * by the original plan is retained by OperatorPruner.
     */
    cout << "Restricted A* found no solution. "
         << "Keeping original plan." << endl;

    return plan;
}