#ifndef PLAN_IMPROVEMENT_ACTION_ELIMINATION_H
#define PLAN_IMPROVEMENT_ACTION_ELIMINATION_H

#include "plan_improver.h"

class ActionElimination : public PlanImprover {
public:
    Plan improve(
        const Plan &plan, const TaskProxy &task_proxy,
        StateRegistry &state_registry) override;
};

#endif