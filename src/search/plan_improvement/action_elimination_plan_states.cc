#include "action_elimination_plan_states.h"

#include "action_elimination.h"

#include "../operator_id.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../task_utils/task_properties.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace task_properties;

struct StateInfo {
    State state;
    size_t index;
    size_t prefix_cost;
};

struct DijkstraEntry {
    size_t node_index;
    size_t cost;
};

struct DijkstraEntryCompare {
    bool operator()(
        const DijkstraEntry &left, const DijkstraEntry &right) const {
        return left.cost > right.cost;
    }
};

static size_t find_graph_node_index(const PlanGraph &graph, StateID state_id) {
    for (size_t i = 0; i < graph.size(); ++i) {
        if (graph[i].state == state_id) {
            return i;
        }
    }

    return graph.size();
}

static Plan find_shortest_plan(
    const PlanGraph &graph, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    const size_t infinity = numeric_limits<size_t>::max();

    const size_t no_node = graph.size();

    State initial_state = state_registry.get_initial_state();

    size_t initial_index = find_graph_node_index(graph, initial_state.get_id());

    if (initial_index == no_node) {
        cerr << "ERROR: Initial state is not in graph." << endl;

        return {};
    }

    vector<size_t> distance(graph.size(), infinity);

    vector<size_t> predecessor(graph.size(), no_node);

    vector<OperatorID> predecessor_action;

    vector<size_t> predecessor_action_index(graph.size(), infinity);

    priority_queue<DijkstraEntry, vector<DijkstraEntry>, DijkstraEntryCompare>
        open;

    distance[initial_index] = 0;

    open.push({initial_index, 0});

    size_t goal_index = no_node;

    while (!open.empty()) {
        DijkstraEntry current = open.top();

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
                find_graph_node_index(graph, edge.end_state);

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
        cerr << "ERROR: No goal state reachable "
             << "in candidate graph." << endl;

        return {};
    }

    cout << "Shortest graph path cost: " << distance[goal_index] << endl;

    Plan reversed_plan;

    size_t current_index = goal_index;

    while (current_index != initial_index) {
        size_t action_index = predecessor_action_index[current_index];

        if (action_index == infinity) {
            cerr << "ERROR: Missing predecessor action "
                 << "during graph path reconstruction." << endl;

            return {};
        }

        reversed_plan.push_back(predecessor_action[action_index]);

        current_index = predecessor[current_index];

        if (current_index == no_node) {
            cerr << "ERROR: Missing predecessor "
                 << "during graph path reconstruction." << endl;

            return {};
        }
    }

    Plan result_plan;

    for (size_t i = reversed_plan.size(); i > 0; --i) {
        result_plan.push_back(reversed_plan[i - 1]);
    }

    return result_plan;
}

static vector<StateInfo> extract_state_info_from_plan(
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

static unordered_map<int, vector<size_t>> build_state_position_lookup(
    const vector<StateInfo> &plan_states) {
    unordered_map<int, vector<size_t>> state_positions;

    for (size_t k = 0; k < plan_states.size(); ++k) {
        int state_id = plan_states[k].state.get_id().get_value();

        state_positions[state_id].push_back(k);
    }

    return state_positions;
}

vector<ReductionCandidate> ActionEliminationPlanStates::candidate_extractor(
    const Plan &plan, const shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry, bool apply_reductions) {
    vector<ReductionCandidate> reduction_candidates;

    TaskProxy task_proxy(*task);
    OperatorsProxy operators = task_proxy.get_operators();

    Plan current_plan = plan;

    vector<StateInfo> plan_states =
        extract_state_info_from_plan(current_plan, task_proxy, state_registry);

    /*
     * Maps:
     *
     * StateID -> all positions in the current plan
     *            where this state occurs.
     *
     * The vectors of positions are automatically sorted
     * because we insert them in increasing order.
     */
    unordered_map<int, vector<size_t>> state_positions =
        build_state_position_lookup(plan_states);

    size_t i = 0;

    while (i < current_plan.size()) {
        State simulated_state = plan_states[i].state;

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

            candidate_cost += static_cast<size_t>(op.get_cost());

            /*
             * Instead of scanning:
             *
             *     k = j + 1 ... plan_states.size()
             *
             * look up only the positions that contain
             * simulated_state.
             */
            int simulated_state_id = simulated_state.get_id().get_value();

            unordered_map<int, vector<size_t>>::const_iterator state_it =
                state_positions.find(simulated_state_id);

            /*
             * This simulated state does not occur anywhere
             * on the current plan.
             */
            if (state_it == state_positions.end()) {
                continue;
            }

            const vector<size_t> &matching_positions = state_it->second;

            /*
             * We only want reconnect positions k > j.
             *
             * Because matching_positions is sorted,
             * upper_bound jumps directly to the first
             * matching position greater than j.
             */
            vector<size_t>::const_iterator k_it = upper_bound(
                matching_positions.begin(), matching_positions.end(), j);

            for (; k_it != matching_positions.end(); ++k_it) {
                size_t k = *k_it;

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

                /*
                 * Static pass:
                 *
                 * Store all candidates and keep searching.
                 */
                if (!apply_reductions) {
                    continue;
                }

                /*
                 * Greedy pass:
                 *
                 * Apply the first reduction found.
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

                /*
                 * Since the plan changed, both the plan-state
                 * information AND the lookup table are now
                 * outdated.
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
         * If we applied a reduction, retry the same i
         * on the changed plan.
         */
        if (reduction_applied) {
            continue;
        }

        ++i;
    }

    return reduction_candidates;
}

static PlanGraphNode *find_graph_node(PlanGraph &graph, StateID state_id) {
    for (PlanGraphNode &node : graph) {
        if (node.state == state_id) {
            return &node;
        }
    }
    return nullptr;
}

vector<ReductionCandidate> ActionEliminationPlanStates::ae_candidate_extractor(
    const Plan &plan, const shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry, bool apply_reductions) {
    vector<ReductionCandidate> reduction_candidates;

    TaskProxy task_proxy(*task);
    OperatorsProxy operators = task_proxy.get_operators();

    Plan current_plan = plan;

    State initial_state = state_registry.get_initial_state();

    size_t i = 0;

    while (i < current_plan.size()) {
        State prefix_state = initial_state;

        for (size_t prefix_index = 0; prefix_index < i; ++prefix_index) {
            OperatorProxy prefix_operator =
                operators[current_plan[prefix_index]];

            prefix_state = state_registry.get_successor_state(
                prefix_state, prefix_operator);
        }
        State current_state = prefix_state;

        Plan candidate_segment;
        size_t candidate_cost = 0;
        for (size_t j = i + 1; j < current_plan.size(); ++j) {
            OperatorProxy current_operator = operators[current_plan[j]];

            if (!is_applicable(current_operator, current_state)) {
                continue;
            }

            current_state = state_registry.get_successor_state(
                current_state, current_operator);

            candidate_segment.push_back(current_plan[j]);

            candidate_cost += static_cast<size_t>(current_operator.get_cost());
        }

        if (is_goal_state(task_proxy, current_state)) {
            ReductionCandidate candidate{
                i,
                current_plan.size(),
                prefix_state.get_id(),
                current_state.get_id(),
                candidate_segment,
                candidate_cost};

            reduction_candidates.push_back(candidate);

            if (!apply_reductions) {
                ++i;
                continue;
            }

            Plan improved_plan;

            improved_plan.insert(
                improved_plan.end(), current_plan.begin(),
                current_plan.begin() + i);

            improved_plan.insert(
                improved_plan.end(), candidate_segment.begin(),
                candidate_segment.end());

            current_plan = std::move(improved_plan);

            continue;
        }
        ++i;
    }

    return reduction_candidates;
}

static PlanGraphNode &get_or_create_graph_node(
    PlanGraph &graph, StateID state_id) {
    PlanGraphNode *node = find_graph_node(graph, state_id);

    if (node != nullptr) {
        return *node;
    }

    graph.push_back({state_id, {}});

    return graph.back();
}

static void add_edge(
    PlanGraph &graph, StateID start_state, StateID end_state, OperatorID action,
    size_t cost) {
    PlanGraphNode &node = get_or_create_graph_node(graph, start_state);

    /*
     * Same state + same operator means exactly
     * the same deterministic transition.
     */
    for (const PlanGraphEdge &edge : node.outgoing_edges) {
        if (edge.action == action) {
            return;
        }
    }

    node.outgoing_edges.push_back({end_state, action, cost});

    get_or_create_graph_node(graph, end_state);
}

static size_t count_graph_edges(const PlanGraph &graph) {
    size_t count = 0;

    for (const PlanGraphNode &node : graph) {
        count += node.outgoing_edges.size();
    }

    return count;
}

PlanGraph ActionEliminationPlanStates::build_graph(
    const Plan &plan, const vector<ReductionCandidate> &candidates,
    const TaskProxy &task_proxy, StateRegistry &state_registry) {
    PlanGraph graph;

    OperatorsProxy operators = task_proxy.get_operators();

    State current_state = state_registry.get_initial_state();

    get_or_create_graph_node(graph, current_state.get_id());

    for (OperatorID op_id : plan) {
        OperatorProxy op = operators[op_id];

        State next_state =
            state_registry.get_successor_state(current_state, op);

        add_edge(
            graph, current_state.get_id(), next_state.get_id(), op_id,
            static_cast<size_t>(op.get_cost()));

        current_state = next_state;
    }

    for (const ReductionCandidate &candidate : candidates) {
        State candidate_state =
            state_registry.lookup_state(candidate.start_state);

        for (OperatorID op_id : candidate.replacement) {
            OperatorProxy op = operators[op_id];

            State next_state =
                state_registry.get_successor_state(candidate_state, op);

            add_edge(
                graph, candidate_state.get_id(), next_state.get_id(), op_id,
                static_cast<size_t>(op.get_cost()));

            candidate_state = next_state;
        }
    }

    return graph;
}

Plan ActionEliminationPlanStates::improve(
    const Plan &plan, const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    TaskProxy task_proxy(*task);
    OperatorsProxy operators = task_proxy.get_operators();
    cout << "\n\n\n!!!!! I am at the start of ae_plan_states !!!!\n" << endl;
    vector<ReductionCandidate> reduction_candidates =
        candidate_extractor(plan, task, state_registry, false);
    cout << "Number of reduction candidates after the first extractor: "
         << reduction_candidates.size() << endl;
    vector<ReductionCandidate> greedy_plan_state_candidates =
        candidate_extractor(plan, task, state_registry, true);

    vector<ReductionCandidate> ae_candidates =
        ae_candidate_extractor(plan, task, state_registry, false);

    vector<ReductionCandidate> greedy_ae_candidates =
        ae_candidate_extractor(plan, task, state_registry, true);

    reduction_candidates.insert(
        reduction_candidates.end(), greedy_plan_state_candidates.begin(),
        greedy_plan_state_candidates.end());

    cout << "Number of reduction candidates after the greedy: "
         << reduction_candidates.size() << endl;

    reduction_candidates.insert(
        reduction_candidates.end(), ae_candidates.begin(), ae_candidates.end());

    cout << "Number of reduction candidates after the ae candidates: "
         << reduction_candidates.size() << endl;

    reduction_candidates.insert(
        reduction_candidates.end(), greedy_ae_candidates.begin(),
        greedy_ae_candidates.end());

    cout << "Number of reduction candidates after the greedy ae candidates: "
         << reduction_candidates.size() << endl;
    PlanGraph graph =
        build_graph(plan, reduction_candidates, task_proxy, state_registry);

    cout << "Graph states: " << graph.size() << endl;

    cout << "Graph edges: " << count_graph_edges(graph) << endl;

    // Now lets run Dijkstra to find the shortest plan.
    Plan improved_plan = find_shortest_plan(graph, task_proxy, state_registry);

    cout << "Original plan length: " << plan.size() << endl;

    cout << "Graph plan length: " << improved_plan.size() << "\n\n" << endl;

    return improved_plan;
}