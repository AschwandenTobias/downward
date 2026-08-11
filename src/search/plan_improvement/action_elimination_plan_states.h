#ifndef PLAN_IMPROVEMENT_ACTION_ELIMINATION_PLAN_STATES_H
#define PLAN_IMPROVEMENT_ACTION_ELIMINATION_PLAN_STATES_H

#include "plan_improver.h"

class ActionEliminationPlanStates : public PlanImprover {
public:
    Plan improve(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry) override;
};

#endif