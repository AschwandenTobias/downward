#ifndef GREEDY_PLAN_STATES_H
#define GREEDY_PLAN_STATES_H

#include "plan_improver.h"

class GreedyPlanStates : public PlanImprover {
public:
    Plan improve(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry) override;
};

#endif