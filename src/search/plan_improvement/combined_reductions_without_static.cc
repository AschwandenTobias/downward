#include "combined_reductions_without_static.h"

#include "action_elimination_plan_states.h"

#include "../state_registry.h"
#include "../task_proxy.h"

#include "../task_utils/task_properties.h"

#include <chrono>
#include <iostream>
#include <limits>
#include <queue>
#include <vector>

using namespace std;
using namespace task_properties;

struct WithoutStaticDijkstraEntry {
    size_t node_index;
    size_t cost;
};

struct WithoutStaticDijkstraEntryCompare {
    bool operator()(
        const WithoutStaticDijkstraEntry &left,
        const WithoutStaticDijkstraEntry &right) const {
        return left.cost > right.cost;
    }
};

static size_t find_graph_node_index_without_static(
    const PlanGraph &graph, StateID state_id) {
    for (size_t i = 0; i < graph.size(); ++i) {
        if (graph[i].state == state_id) {
            return i;
        }
    }

    return graph.size();
}

static size_t count_graph_edges_without_static(const PlanGraph &graph) {
    size_t count = 0;

    for (const PlanGraphNode &node : graph) {
        count += node.outgoing_edges.size();
    }

    return count;
}

static Plan find_shortest_plan_without_static(
    const PlanGraph &graph, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    const size_t infinity = numeric_limits<size_t>::max();

    const size_t no_node = graph.size();

    State initial_state = state_registry.get_initial_state();

    size_t initial_index =
        find_graph_node_index_without_static(graph, initial_state.get_id());

    vector<size_t> distance(graph.size(), infinity);

    vector<size_t> predecessor(graph.size(), no_node);

    vector<OperatorID> predecessor_action;

    vector<size_t> predecessor_action_index(graph.size(), infinity);

    priority_queue<
        WithoutStaticDijkstraEntry, vector<WithoutStaticDijkstraEntry>,
        WithoutStaticDijkstraEntryCompare>
        open;

    distance[initial_index] = 0;

    open.push({initial_index, 0});

    size_t goal_index = no_node;

    while (!open.empty()) {
        WithoutStaticDijkstraEntry current = open.top();

        open.pop();

        size_t current_index = current.node_index;

        if (current.cost != distance[current_index]) {
            continue;
        }

        State current_state =
            state_registry.lookup_state(graph[current_index].state);

        if (is_goal_state(task_proxy, current_state)) {
            goal_index = current_index;

            break;
        }

        const PlanGraphNode &current_node = graph[current_index];

        for (const PlanGraphEdge &edge : current_node.outgoing_edges) {
            size_t successor_index =
                find_graph_node_index_without_static(graph, edge.end_state);

            if (successor_index == no_node) {
                cerr << "ERROR: Edge points to state "
                     << "that is not stored as graph node." << endl;

                continue;
            }

            size_t new_cost = distance[current_index] + edge.cost;

            if (new_cost >= distance[successor_index]) {
                continue;
            }

            distance[successor_index] = new_cost;

            predecessor[successor_index] = current_index;

            predecessor_action.push_back(edge.action);

            predecessor_action_index[successor_index] =
                predecessor_action.size() - 1;

            open.push({successor_index, new_cost});
        }
    }

    if (goal_index == no_node) {
        cerr << "ERROR: No goal state found in graph." << endl;

        return {};
    }

    cout << "Shortest graph path cost: " << distance[goal_index] << endl;

    Plan reversed_plan;

    size_t current_index = goal_index;

    while (current_index != initial_index) {
        size_t action_index = predecessor_action_index[current_index];

        reversed_plan.push_back(predecessor_action[action_index]);

        current_index = predecessor[current_index];
    }

    Plan result_plan;

    for (size_t i = reversed_plan.size(); i > 0; --i) {
        result_plan.push_back(reversed_plan[i - 1]);
    }

    return result_plan;
}

Plan CombinedReductionsWithoutStatic::improve(
    const Plan &plan, const shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    TaskProxy task_proxy(*task);

    vector<ReductionCandidate> reduction_candidates;

    chrono::steady_clock::time_point greedy_plan_states_start =
        chrono::steady_clock::now();

    vector<ReductionCandidate> greedy_plan_state_candidates =
        candidate_extractor(plan, task, state_registry, true, 0);

    chrono::steady_clock::time_point greedy_plan_states_end =
        chrono::steady_clock::now();

    cout << "Greedy plan-state time: "
         << chrono::duration<double>(
                greedy_plan_states_end - greedy_plan_states_start)
                .count()
         << "s" << endl;

    cout << "Greedy plan-state candidates: "
         << greedy_plan_state_candidates.size() << endl;

    reduction_candidates.insert(
        reduction_candidates.end(), greedy_plan_state_candidates.begin(),
        greedy_plan_state_candidates.end());

    chrono::steady_clock::time_point greedy_ae_start =
        chrono::steady_clock::now();

    vector<ReductionCandidate> greedy_ae_candidates =
        ae_candidate_extractor(plan, task, state_registry, true, 0);

    chrono::steady_clock::time_point greedy_ae_end =
        chrono::steady_clock::now();

    cout << "Greedy AE time: "
         << chrono::duration<double>(greedy_ae_end - greedy_ae_start).count()
         << "s" << endl;

    cout << "Greedy AE candidates: " << greedy_ae_candidates.size() << endl;

    reduction_candidates.insert(
        reduction_candidates.end(), greedy_ae_candidates.begin(),
        greedy_ae_candidates.end());

    cout << "Total candidates: " << reduction_candidates.size() << endl;

    chrono::steady_clock::time_point graph_start = chrono::steady_clock::now();

    PlanGraph graph =
        build_graph(plan, reduction_candidates, task_proxy, state_registry);

    chrono::steady_clock::time_point graph_end = chrono::steady_clock::now();

    cout << "Graph construction time: "
         << chrono::duration<double>(graph_end - graph_start).count() << "s"
         << endl;

    cout << "Graph states: " << graph.size() << endl;

    cout << "Graph edges: " << count_graph_edges_without_static(graph) << endl;

    chrono::steady_clock::time_point dijkstra_start =
        chrono::steady_clock::now();

    Plan improved_plan =
        find_shortest_plan_without_static(graph, task_proxy, state_registry);

    chrono::steady_clock::time_point dijkstra_end = chrono::steady_clock::now();

    cout << "Dijkstra time: "
         << chrono::duration<double>(dijkstra_end - dijkstra_start).count()
         << "s" << endl;

    cout << "Original plan length: " << plan.size() << endl;

    cout << "Graph plan length: " << improved_plan.size() << "\n\n" << endl;

    return improved_plan;
}