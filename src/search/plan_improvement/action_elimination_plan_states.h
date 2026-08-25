#ifndef PLAN_IMPROVEMENT_ACTION_ELIMINATION_PLAN_STATES_H
#define PLAN_IMPROVEMENT_ACTION_ELIMINATION_PLAN_STATES_H

#include "plan_improver.h"

#include "../state_id.h"

#include <memory>
#include <vector>

// AE: might change the startState and endState to ID's?
struct ReductionCandidate {
    size_t start_index;
    size_t end_index;
    StateID start_state;
    StateID end_state;
    Plan replacement;
    size_t cost;
};
class ActionEliminationPlanStates : public PlanImprover {
public:
    Plan improve(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry) override;
    std::vector<ReductionCandidate> candidate_extractor(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry, bool applyReductions);
};

#endif