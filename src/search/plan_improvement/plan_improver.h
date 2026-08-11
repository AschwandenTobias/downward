#ifndef PLAN_IMPROVEMENT_PLAN_IMPROVER_H
#define PLAN_IMPROVEMENT_PLAN_IMPROVER_H

#include "../abstract_task.h"
#include "../plan_manager.h"

#include <memory>

class TaskProxy;
class StateRegistry;

class PlanImprover {
public:
    virtual Plan improve(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry) = 0;

    virtual ~PlanImprover() = default;
};

#endif