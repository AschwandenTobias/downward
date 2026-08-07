#ifndef PLAN_IMPROVEMENT_PLAN_IMPROVER_H
#define PLAN_IMPROVEMENT_PLAN_IMPROVER_H

#include "../plan_manager.h"

class TaskProxy;
class StateRegistry;

class PlanImprover {
public:
    virtual Plan improve(
        const Plan &plan, const TaskProxy &task_proxy,
        StateRegistry &state_registry) = 0;

    virtual ~PlanImprover() = default;
};

#endif