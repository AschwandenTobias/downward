#ifndef ACTION_ELIMINATION_H
#define ACTION_ELIMINATION_H

#include "../operator_id.h"
#include "../plan_manager.h"
#include "../task_proxy.h"

Plan action_elimination(const Plan &plan, const TaskProxy &taskProxy);

#endif
