#include "action_elimination_plan_states.h"

#include "action_elimination.h"

#include "../operator_id.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../task_utils/task_properties.h"

#include <algorithm>
#include <chrono>
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
    StateRegistry &state_registry, bool apply_reductions, size_t start_index) {
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

    size_t i = start_index;

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
    StateRegistry &state_registry, bool apply_reductions, size_t start_index) {
    vector<ReductionCandidate> reduction_candidates;

    TaskProxy task_proxy(*task);
    OperatorsProxy operators = task_proxy.get_operators();

    Plan current_plan = plan;

    State prefix_state = state_registry.get_initial_state();

    /*
     * We only have to compute the prefix once in order to
     * reach start_index.
     */
    for (size_t prefix_index = 0; prefix_index < start_index; ++prefix_index) {
        OperatorProxy prefix_operator = operators[current_plan[prefix_index]];

        prefix_state =
            state_registry.get_successor_state(prefix_state, prefix_operator);
    }

    size_t i = start_index;

    while (i < current_plan.size()) {
        /*
         * Try deleting current_plan[i].
         *
         * prefix_state is already exactly the state before i.
         */
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

            /*
             * Static mode:
             *
             * We do NOT change current_plan.
             *
             * Therefore action i still belongs to the prefix
             * when we move on to i + 1.
             */
            if (!apply_reductions) {
                OperatorProxy prefix_operator = operators[current_plan[i]];

                prefix_state = state_registry.get_successor_state(
                    prefix_state, prefix_operator);

                ++i;

                continue;
            }

            /*
             * Greedy mode:
             *
             * Replace the current plan by:
             *
             * prefix [0, i)
             * +
             * applicable suffix actions.
             */
            Plan improved_plan;

            improved_plan.insert(
                improved_plan.end(), current_plan.begin(),
                current_plan.begin() + i);

            improved_plan.insert(
                improved_plan.end(), candidate_segment.begin(),
                candidate_segment.end());

            current_plan = std::move(improved_plan);

            /*
             * Important:
             *
             * We retry the SAME i.
             *
             * The prefix [0, i) did not change, so prefix_state
             * is still exactly correct. No recomputation needed.
             */
            continue;
        }

        /*
         * No reduction was possible at i.
         *
         * Therefore action i remains part of the plan.
         * Advance prefix_state by applying it once.
         */
        OperatorProxy prefix_operator = operators[current_plan[i]];

        prefix_state =
            state_registry.get_successor_state(prefix_state, prefix_operator);

        ++i;
    }

    return reduction_candidates;
}

static size_t get_or_create_graph_node(
    PlanGraph &graph, unordered_map<int, size_t> &node_indices,
    StateID state_id) {
    int state_value = state_id.get_value();

    unordered_map<int, size_t>::const_iterator it =
        node_indices.find(state_value);

    if (it != node_indices.end()) {
        return it->second;
    }

    size_t new_index = graph.size();

    graph.push_back({state_id, {}});

    node_indices[state_value] = new_index;

    return new_index;
}

static void add_edge(
    PlanGraph &graph, unordered_map<int, size_t> &node_indices,
    StateID start_state, StateID end_state, OperatorID action, size_t cost) {
    size_t start_index =
        get_or_create_graph_node(graph, node_indices, start_state);

    get_or_create_graph_node(graph, node_indices, end_state);

    PlanGraphNode &node = graph[start_index];

    for (const PlanGraphEdge &edge : node.outgoing_edges) {
        if (edge.action == action) {
            return;
        }
    }

    node.outgoing_edges.push_back({end_state, action, cost});
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

    unordered_map<int, size_t> node_indices;

    OperatorsProxy operators = task_proxy.get_operators();

    State current_state = state_registry.get_initial_state();

    get_or_create_graph_node(graph, node_indices, current_state.get_id());

    for (OperatorID op_id : plan) {
        OperatorProxy op = operators[op_id];

        State next_state =
            state_registry.get_successor_state(current_state, op);

        add_edge(
            graph, node_indices, current_state.get_id(), next_state.get_id(),
            op_id, static_cast<size_t>(op.get_cost()));

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
                graph, node_indices, candidate_state.get_id(),
                next_state.get_id(), op_id, static_cast<size_t>(op.get_cost()));

            candidate_state = next_state;
        }
    }

    return graph;
}

Plan ActionEliminationPlanStates::improve(
    const Plan &plan, const std::shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    TaskProxy task_proxy(*task);

    cout << "\n\n!!!!! I am at the start of ae_plan_states !!!!!\n" << endl;

    vector<ReductionCandidate> reduction_candidates;

    // ============================================================
    // 1. Greedy plan-state extraction
    // ============================================================

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

    // ============================================================
    // 2. Static plan-state extraction
    //
    // Only necessary if greedy found something.
    // Start where greedy found its first reduction because all
    // earlier positions were already checked on the original plan.
    // ============================================================

    if (!greedy_plan_state_candidates.empty()) {
        size_t first_reduction_index =
            greedy_plan_state_candidates.front().start_index;

        chrono::steady_clock::time_point static_plan_states_start =
            chrono::steady_clock::now();

        vector<ReductionCandidate> static_plan_state_candidates =
            candidate_extractor(
                plan, task, state_registry, false, first_reduction_index);

        chrono::steady_clock::time_point static_plan_states_end =
            chrono::steady_clock::now();

        cout << "Static plan-state time: "
             << chrono::duration<double>(
                    static_plan_states_end - static_plan_states_start)
                    .count()
             << "s" << endl;

        cout << "Static plan-state candidates: "
             << static_plan_state_candidates.size() << endl;

        reduction_candidates.insert(
            reduction_candidates.end(), static_plan_state_candidates.begin(),
            static_plan_state_candidates.end());

    } else {
        cout << "Static plan-state extraction skipped." << endl;
    }

    // The greedy candidates are useful as well.
    reduction_candidates.insert(
        reduction_candidates.end(), greedy_plan_state_candidates.begin(),
        greedy_plan_state_candidates.end());

    // ============================================================
    // 3. Greedy AE extraction
    // ============================================================

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

    // ============================================================
    // 4. Static AE extraction
    // ============================================================

    if (!greedy_ae_candidates.empty()) {
        size_t first_reduction_index = greedy_ae_candidates.front().start_index;

        chrono::steady_clock::time_point static_ae_start =
            chrono::steady_clock::now();

        vector<ReductionCandidate> static_ae_candidates =
            ae_candidate_extractor(
                plan, task, state_registry, false, first_reduction_index);

        chrono::steady_clock::time_point static_ae_end =
            chrono::steady_clock::now();

        cout
            << "Static AE time: "
            << chrono::duration<double>(static_ae_end - static_ae_start).count()
            << "s" << endl;

        cout << "Static AE candidates: " << static_ae_candidates.size() << endl;

        reduction_candidates.insert(
            reduction_candidates.end(), static_ae_candidates.begin(),
            static_ae_candidates.end());

    } else {
        cout << "Static AE extraction skipped." << endl;
    }

    reduction_candidates.insert(
        reduction_candidates.end(), greedy_ae_candidates.begin(),
        greedy_ae_candidates.end());

    cout << "Total candidates: " << reduction_candidates.size() << endl;

    // ============================================================
    // 5. Graph construction
    // ============================================================

    chrono::steady_clock::time_point graph_start = chrono::steady_clock::now();

    PlanGraph graph =
        build_graph(plan, reduction_candidates, task_proxy, state_registry);

    chrono::steady_clock::time_point graph_end = chrono::steady_clock::now();

    cout << "Graph construction time: "
         << chrono::duration<double>(graph_end - graph_start).count() << "s"
         << endl;

    cout << "Graph states: " << graph.size() << endl;

    cout << "Graph edges: " << count_graph_edges(graph) << endl;

    // ============================================================
    // 6. Dijkstra
    // ============================================================

    chrono::steady_clock::time_point dijkstra_start =
        chrono::steady_clock::now();

    Plan improved_plan = find_shortest_plan(graph, task_proxy, state_registry);

    chrono::steady_clock::time_point dijkstra_end = chrono::steady_clock::now();

    cout << "Dijkstra time: "
         << chrono::duration<double>(dijkstra_end - dijkstra_start).count()
         << "s" << endl;

    cout << "Original plan length: " << plan.size() << endl;

    cout << "Graph plan length: " << improved_plan.size() << "\n\n" << endl;

    return improved_plan;
}