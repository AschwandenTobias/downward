#ifndef TASKS_MINIMAL_REDUCTION_TASK_H
#define TASKS_MINIMAL_REDUCTION_TASK_H

#include "delegating_task.h"

#include "../plan_manager.h"

#include <memory>

class MinimalReductionTask : public tasks::DelegatingTask {
private:
    Plan plan;
    int position_variable;

public:
    MinimalReductionTask(
        const std::shared_ptr<AbstractTask> &parent, const Plan &plan);

    int get_num_variables() const override;
    std::string get_variable_name(int var) const override;
    int get_variable_domain_size(int var) const override;
    int get_variable_axiom_layer(int var) const override;
    int get_variable_default_axiom_value(int var) const override;
    std::string get_fact_name(const FactPair &fact) const override;
    bool are_facts_mutex(const FactPair &fact1, const FactPair &fact2) const;
};

#endif