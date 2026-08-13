#ifndef PLAN_IMPROVEMENT_OPERATOR_REDUCTION_H
#define PLAN_IMPROVEMENT_OPERATOR_REDUCTION_H

#include "plan_improver.h"

enum class OperatorReductionType {
    IDS,
    NAMES
};

class OperatorReduction : public PlanImprover {
    OperatorReductionType reduction_type;

public:
    explicit OperatorReduction(OperatorReductionType reduction_type);

    Plan improve(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry) override;
};

#endif