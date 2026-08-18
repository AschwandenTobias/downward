#ifndef TASKS_MINIMAL_REDUCTION_TASK_H
#define TASKS_MINIMAL_REDUCTION_TASK_H

#include "delegating_task.h"

#include "../plan_manager.h"

#include <memory>

class MinimalReductionTask : public tasks::DelegatingTask {
private:
    Plan plan;
    int position_variable;

    bool is_skip_operator(int index) const;
    int get_plan_position(int index) const;
public:
    MinimalReductionTask(
        const std::shared_ptr<AbstractTask> &parent, const Plan &plan);

    int get_num_variables() const override;
    std::string get_variable_name(int var) const override;
    int get_variable_domain_size(int var) const override;
    int get_variable_axiom_layer(int var) const override;
    int get_variable_default_axiom_value(int var) const override;
    std::string get_fact_name(const FactPair &fact) const override;
    bool are_facts_mutex(
        const FactPair &fact1, const FactPair &fact2) const override;
    int get_num_operators() const override;
    int get_operator_cost(int index, bool is_axiom) const override;
    std::string get_operator_name(int index, bool is_axiom) const override;
    int get_num_operator_preconditions(int index, bool is_axiom) const override;
    FactPair get_operator_precondition(
        int op_index, int fact_index, bool is_axiom) const override;
    int get_num_operator_effects(int index, bool is_axiom) const override;
    FactPair get_operator_effect(
        int op_index, int fact_index, bool is_axiom) const override;
    int get_num_operator_effect_conditions(
        int op_index, int effect_index, bool is_axiom) const override;

    FactPair get_operator_effect_condition(
        int op_index, int effect_index, int condition_index,
        bool is_axiom) const override;

    std::vector<int> get_initial_state_values() const override;
};

#endif