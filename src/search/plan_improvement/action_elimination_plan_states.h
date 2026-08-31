#ifndef PLAN_IMPROVEMENT_ACTION_ELIMINATION_PLAN_STATES_H
#define PLAN_IMPROVEMENT_ACTION_ELIMINATION_PLAN_STATES_H

#include "plan_improver.h"

#include "../operator_id.h"
#include "../state_id.h"
#include "../task_proxy.h"

#include <memory>
#include <vector>

struct ReductionCandidate {
    size_t start_index;
    size_t end_index;

    StateID start_state;
    StateID end_state;

    Plan replacement;
    size_t cost;
};

struct PlanGraphEdge {
    StateID end_state;
    OperatorID action;
    size_t cost;
};

struct PlanGraphNode {
    StateID state;
    std::vector<PlanGraphEdge> outgoing_edges;
};

using PlanGraph = std::vector<PlanGraphNode>;

class ActionEliminationPlanStates : public PlanImprover {
private:
    std::vector<ReductionCandidate> candidate_extractor(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry, bool apply_reductions);

    static unordered_map<int, vector<size_t>> build_state_position_lookup(
        const vector<StateInfo> &plan_states);

    std::vector<ReductionCandidate> ae_candidate_extractor(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry, bool apply_reductions);

    PlanGraph build_graph(
        const Plan &plan, const std::vector<ReductionCandidate> &candidates,
        const TaskProxy &task_proxy, StateRegistry &state_registry);

public:
    Plan improve(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry) override;
};

#endif