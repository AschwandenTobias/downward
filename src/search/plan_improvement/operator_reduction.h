#ifndef PLAN_IMPROVEMENT_OPERATOR_REDUCTION_H
#define PLAN_IMPROVEMENT_OPERATOR_REDUCTION_H

#include "plan_improver.h"
class OperatorReduction : public PlanImprover {
public:
    Plan improve(
        const Plan &plan, const TaskProxy &task_proxy,
        StateRegistry &state_registry) override;
};

#endif