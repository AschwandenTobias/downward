#include "plan_neighborhood_graph_search.h"

#include "../state_registry.h"
#include "../task_proxy.h"

#include "../task_utils/task_properties.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <queue>
#include <vector>

using namespace std;
using namespace task_properties;

// ============================================================
// Graph
// ============================================================

struct PNGEdge {
    StateID end_state;
    OperatorID action;
    size_t cost;
};

struct PNGNode {
    StateID state;
    vector<PNGEdge> outgoing_edges;
};

using PNGGraph = vector<PNGNode>;

// ============================================================
// Graph helpers
// ============================================================

static size_t find_graph_node(const PNGGraph &graph, StateID state_id) {
    for (size_t i = 0; i < graph.size(); ++i) {
        if (graph[i].state == state_id) {
            return i;
        }
    }

    return graph.size();
}

static size_t add_graph_node(PNGGraph &graph, StateID state_id) {
    size_t index = find_graph_node(graph, state_id);

    if (index != graph.size()) {
        return index;
    }

    graph.push_back({state_id, {}});

    return graph.size() - 1;
}

static void add_graph_edge(
    PNGGraph &graph, StateID start_state, StateID end_state, OperatorID action,
    size_t cost) {
    size_t start_index = add_graph_node(graph, start_state);

    add_graph_node(graph, end_state);

    // Avoid duplicate identical transitions.
    for (const PNGEdge &edge : graph[start_index].outgoing_edges) {
        if (edge.end_state == end_state && edge.action == action) {
            return;
        }
    }

    graph[start_index].outgoing_edges.push_back({end_state, action, cost});
}

static size_t count_graph_edges(const PNGGraph &graph) {
    size_t count = 0;

    for (const PNGNode &node : graph) {
        count += node.outgoing_edges.size();
    }

    return count;
}

// ============================================================
// Add original plan to graph.
//
// Returns:
//   s_0, s_1, ..., s_n
//
// These are the states around which PNGS will search.
// ============================================================

static vector<StateID> add_original_plan(
    const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry, PNGGraph &graph) {
    OperatorsProxy operators = task_proxy.get_operators();

    vector<StateID> plan_states;

    State current_state = state_registry.get_initial_state();

    plan_states.push_back(current_state.get_id());

    add_graph_node(graph, current_state.get_id());

    for (OperatorID operator_id : plan) {
        OperatorProxy op = operators[operator_id];

        if (!is_applicable(op, current_state)) {
            cerr << "ERROR: Original plan contains "
                 << "an inapplicable operator." << endl;

            return {};
        }

        State successor = state_registry.get_successor_state(current_state, op);

        add_graph_edge(
            graph, current_state.get_id(), successor.get_id(), operator_id,
            static_cast<size_t>(op.get_cost()));

        current_state = successor;

        plan_states.push_back(current_state.get_id());
    }

    return plan_states;
}

// ============================================================
// Local neighborhood search
//
// Starting from one plan state, explore the state space using
// uniform-cost search.
//
// Each newly selected state contributes:
//   parent --operator--> state
//
// to the plan-neighborhood graph.
//
// Stop after L new states have been selected.
//
// IMPORTANT:
// This explicitly iterates over ALL task operators, exactly as
// your plan-improvement code can already do.
// ============================================================

struct LocalSearchNode {
    StateID state;
    size_t cost;
    size_t parent;
    OperatorID action;
};

struct LocalOpenEntry {
    size_t node_index;
    size_t cost;
};

struct LocalOpenCompare {
    bool operator()(
        const LocalOpenEntry &left, const LocalOpenEntry &right) const {
        return left.cost > right.cost;
    }
};

static size_t find_local_node(
    const vector<LocalSearchNode> &nodes, StateID state_id) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].state == state_id) {
            return i;
        }
    }

    return nodes.size();
}

static void add_neighborhood(
    StateID start_state_id, size_t neighborhood_size,
    const TaskProxy &task_proxy, StateRegistry &state_registry,
    PNGGraph &graph) {
    if (neighborhood_size == 0) {
        return;
    }

    OperatorsProxy operators = task_proxy.get_operators();

    const size_t no_parent = numeric_limits<size_t>::max();

    vector<LocalSearchNode> nodes;

    nodes.push_back({start_state_id, 0, no_parent, OperatorID(-1)});

    priority_queue<LocalOpenEntry, vector<LocalOpenEntry>, LocalOpenCompare>
        open;

    open.push({0, 0});

    vector<bool> closed;
    closed.push_back(false);

    size_t added_states = 0;

    while (!open.empty() && added_states < neighborhood_size) {
        LocalOpenEntry entry = open.top();

        open.pop();

        size_t current_index = entry.node_index;

        if (closed[current_index]) {
            continue;
        }

        if (entry.cost != nodes[current_index].cost) {
            continue;
        }

        closed[current_index] = true;

        // ----------------------------------------------------
        // Every state except the start state corresponds to
        // one transition discovered by the local search.
        // ----------------------------------------------------

        if (current_index != 0) {
            const LocalSearchNode &node = nodes[current_index];

            const LocalSearchNode &parent = nodes[node.parent];

            OperatorProxy op = operators[node.action];

            add_graph_edge(
                graph, parent.state, node.state, node.action,
                static_cast<size_t>(op.get_cost()));

            ++added_states;

            if (added_states >= neighborhood_size) {
                break;
            }
        }

        State current_state =
            state_registry.lookup_state(nodes[current_index].state);

        // ====================================================
        // Generate successors manually.
        //
        // This is the important Fast Downward part:
        //
        //     for every operator
        //         check is_applicable(...)
        //         get_successor_state(...)
        //
        // No SuccessorGenerator is needed.
        // ====================================================

        for (size_t operator_index = 0; operator_index < operators.size();
             ++operator_index) {
            OperatorProxy op = operators[operator_index];
            OperatorID operator_id(static_cast<int>(operator_index));

            if (!is_applicable(op, current_state)) {
                continue;
            }

            State successor =
                state_registry.get_successor_state(current_state, op);

            StateID successor_id = successor.get_id();

            size_t successor_index = find_local_node(nodes, successor_id);

            size_t new_cost =
                nodes[current_index].cost + static_cast<size_t>(op.get_cost());

            if (successor_index == nodes.size()) {
                nodes.push_back(
                    LocalSearchNode{
                        successor_id, new_cost, current_index, operator_id});

                closed.push_back(false);

                open.push({nodes.size() - 1, new_cost});

                continue;
            }

            if (closed[successor_index]) {
                continue;
            }

            if (new_cost >= nodes[successor_index].cost) {
                continue;
            }

            nodes[successor_index].cost = new_cost;

            nodes[successor_index].parent = current_index;

            nodes[successor_index].action = operator_id;

            open.push({successor_index, new_cost});
        }
    }
}

// ============================================================
// Dijkstra over final PNG
// ============================================================

struct DijkstraEntry {
    size_t node_index;
    size_t cost;
};

struct DijkstraCompare {
    bool operator()(
        const DijkstraEntry &left, const DijkstraEntry &right) const {
        return left.cost > right.cost;
    }
};

static Plan find_shortest_plan(
    const PNGGraph &graph, const TaskProxy &task_proxy,
    StateRegistry &state_registry) {
    const size_t infinity = numeric_limits<size_t>::max();

    const size_t no_node = graph.size();

    State initial_state = state_registry.get_initial_state();

    size_t initial_index = find_graph_node(graph, initial_state.get_id());

    if (initial_index == no_node) {
        cerr << "ERROR: Initial state missing from PNG." << endl;

        return {};
    }

    vector<size_t> distance(graph.size(), infinity);

    vector<size_t> predecessor(graph.size(), no_node);

    vector<size_t> predecessor_edge(graph.size(), infinity);

    priority_queue<DijkstraEntry, vector<DijkstraEntry>, DijkstraCompare> open;

    distance[initial_index] = 0;

    open.push({initial_index, 0});

    size_t goal_index = no_node;

    while (!open.empty()) {
        DijkstraEntry current = open.top();

        open.pop();

        if (current.cost != distance[current.node_index]) {
            continue;
        }

        State current_state =
            state_registry.lookup_state(graph[current.node_index].state);

        if (is_goal_state(task_proxy, current_state)) {
            goal_index = current.node_index;

            break;
        }

        const PNGNode &node = graph[current.node_index];

        for (size_t edge_index = 0; edge_index < node.outgoing_edges.size();
             ++edge_index) {
            const PNGEdge &edge = node.outgoing_edges[edge_index];

            size_t successor_index = find_graph_node(graph, edge.end_state);

            if (successor_index == no_node) {
                continue;
            }

            size_t new_cost = distance[current.node_index] + edge.cost;

            if (new_cost >= distance[successor_index]) {
                continue;
            }

            distance[successor_index] = new_cost;

            predecessor[successor_index] = current.node_index;

            predecessor_edge[successor_index] = edge_index;

            open.push({successor_index, new_cost});
        }
    }

    if (goal_index == no_node) {
        cerr << "ERROR: No goal state reachable in PNG." << endl;

        return {};
    }

    cout << "PNGS shortest path cost: " << distance[goal_index] << endl;

    // ========================================================
    // Reconstruct operator sequence
    // ========================================================

    Plan reversed_plan;

    size_t current_index = goal_index;

    while (current_index != initial_index) {
        size_t parent = predecessor[current_index];

        size_t edge_index = predecessor_edge[current_index];

        if (parent == no_node || edge_index == infinity) {
            cerr << "ERROR: Broken predecessor chain." << endl;

            return {};
        }

        const PNGEdge &edge = graph[parent].outgoing_edges[edge_index];

        reversed_plan.push_back(edge.action);

        current_index = parent;
    }

    reverse(reversed_plan.begin(), reversed_plan.end());

    return reversed_plan;
}

// ============================================================
// Constructor
// ============================================================

PlanNeighborhoodGraphSearch::PlanNeighborhoodGraphSearch(
    size_t neighborhood_size)
    : neighborhood_size(neighborhood_size) {
}

// ============================================================
// Main entry point
// ============================================================

Plan PlanNeighborhoodGraphSearch::improve(
    const Plan &plan, const shared_ptr<AbstractTask> &task,
    StateRegistry &state_registry) {
    TaskProxy task_proxy(*task);

    chrono::steady_clock::time_point start = chrono::steady_clock::now();

    PNGGraph graph;

    // ========================================================
    // 1. Initial graph = original plan
    // ========================================================

    vector<StateID> plan_states =
        add_original_plan(plan, task_proxy, state_registry, graph);

    if (plan_states.empty()) {
        return plan;
    }

    cout << "PNGS neighborhood size: " << neighborhood_size << endl;

    cout << "PNGS original plan states: " << plan_states.size() << endl;

    // ========================================================
    // 2. Construct neighborhood around every state of
    //    original plan.
    //
    // Notice that we only iterate over plan_states.
    // States newly added to graph do NOT themselves become
    // neighborhood roots.
    // ========================================================

    for (StateID state_id : plan_states) {
        add_neighborhood(
            state_id, neighborhood_size, task_proxy, state_registry, graph);
    }

    chrono::steady_clock::time_point graph_end = chrono::steady_clock::now();

    cout << "PNGS graph construction time: "
         << chrono::duration<double>(graph_end - start).count() << "s" << endl;

    cout << "PNGS graph states: " << graph.size() << endl;

    cout << "PNGS graph edges: " << count_graph_edges(graph) << endl;

    // ========================================================
    // 3. Cheapest path through graph
    // ========================================================

    chrono::steady_clock::time_point dijkstra_start =
        chrono::steady_clock::now();

    Plan improved_plan = find_shortest_plan(graph, task_proxy, state_registry);

    chrono::steady_clock::time_point dijkstra_end = chrono::steady_clock::now();

    cout << "PNGS Dijkstra time: "
         << chrono::duration<double>(dijkstra_end - dijkstra_start).count()
         << "s" << endl;

    cout << "Original plan length: " << plan.size() << endl;

    cout << "PNGS plan length: " << improved_plan.size() << endl;

    if (improved_plan.empty() && !plan.empty()) {
        cerr << "WARNING: PNGS failed; "
             << "keeping original plan." << endl;

        return plan;
    }

    return improved_plan;
}