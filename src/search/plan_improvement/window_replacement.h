#ifndef ACTION_ELIMINATION_WINDOW_REPLACEMENT_H
#define ACTION_ELIMINATION_WINDOW_REPLACEMENT_H

#include "plan_improver.h"

class ActionEliminationWindowReplacement : public PlanImprover {
public:
    Plan improve(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry) override;
};

#endif