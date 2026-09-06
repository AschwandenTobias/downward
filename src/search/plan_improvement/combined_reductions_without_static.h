#ifndef COMBINED_REDUCTIONS_WITHOUT_STATIC_H
#define COMBINED_REDUCTIONS_WITHOUT_STATIC_H

#include "action_elimination_plan_states.h"

class CombinedReductionsWithoutStatic : public ActionEliminationPlanStates {
public:
    Plan improve(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry) override;
};

#endif