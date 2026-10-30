#ifndef PLAN_IMPROVEMENT_RECONNECTION_REDUCTION_H
#define PLAN_IMPROVEMENT_RECONNECTION_REDUCTION_H

#include "plan_improvement.h"

namespace reconnection_reduction {
class ReconnectionReduction : public PlanImprovement {
    int reduction_attempts = 0;
    int successfull_reductions = 0;
public:
    Plan improve(
        const Plan &plan, const TaskProxy &task_proxy,
    StateRegistry &state_registry) override;
    void print_statistics() const override;
};
}
#endif