#include "action_elimination_plan_states.h"

#include "action_elimination.h"

#include "../operator_id.h"
#include "../state_registry.h"
#include "../task_proxy.h"

#include "../task_utils/task_properties.h"

#include <limits>
#include <queue>
#include <set>
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

static PlanGraphNode *find_graph_node(PlanGraph &graph, StateID state_id) {
    for (PlanGraphNode &node : graph) {
        if (node.state == state_id) {
            return &node;
        }
    }

    return nullptr;
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

    /*
     * Also create the target node so goal states or
     * dead ends are represented in the graph.
     */
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

    // ---------------------------------------------------------
    // Add the original plan to the graph.
    // ---------------------------------------------------------

    State current_state = state_registry.get_initial_state();

    // Also create the initial state as a node.
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

    // ---------------------------------------------------------
    // Add all candidate paths to the graph.
    // ---------------------------------------------------------

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

        // Debugging check:
        // replaying the candidate should end exactly
        // at the state stored in the candidate.
        if (candidate_state.get_id() != candidate.end_state) {
            cerr << "ERROR: Candidate end state mismatch." << endl;
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
    cout << "Number of reduction candidates: " << reduction_candidates.size()
         << endl;
    vector<ReductionCandidate> greedy_candidates =
        candidate_extractor(plan, task, state_registry, true);

    reduction_candidates.insert(
        reduction_candidates.end(), greedy_candidates.begin(),
        greedy_candidates.end());

    cout << "Number of reduction candidates after the greedy: "
         << reduction_candidates.size() << endl;

    PlanGraph graph =
        build_graph(plan, reduction_candidates, task_proxy, state_registry);

    cout << "Candidates: " << reduction_candidates.size() << endl;

    cout << "Graph states: " << graph.size() << endl;

    cout << "Graph edges: " << count_graph_edges(graph) << endl;

    // Now lets run Dijkstra to find the shortest plan.
    Plan improved_plan = find_shortest_plan(graph, task_proxy, state_registry);

    cout << "Original plan length: " << plan.size() << endl;

    cout << "Graph plan length: " << improved_plan.size() << "\n\n" << endl;

    return improved_plan;
}