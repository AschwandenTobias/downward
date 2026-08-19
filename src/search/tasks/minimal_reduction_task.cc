#include "minimal_reduction_task.h"

using namespace std;

MinimalReductionTask::MinimalReductionTask(
    const shared_ptr<AbstractTask> &parent, const Plan &plan)
    : tasks::DelegatingTask(parent),
      plan(plan),
      position_variable(parent->get_num_variables()) {
}

int MinimalReductionTask::get_num_variables() const {
    return parent->get_num_variables() + 1;
}

string MinimalReductionTask::get_variable_name(int var) const {
    if (var == position_variable) {
        return "plan_position";
    } else {
        return parent->get_variable_name(var);
    }
}

int MinimalReductionTask::get_variable_domain_size(int var) const {
    if (var == position_variable) {
        return plan.size() + 1;
    } else {
        return parent->get_variable_domain_size(var);
    }
}

int MinimalReductionTask::get_variable_axiom_layer(int var) const {
    if (var == position_variable) {
        return 0;
    } else {
        return parent->get_variable_axiom_layer(var);
    }
}

int MinimalReductionTask::get_variable_default_axiom_value(int var) const {
    if (var == position_variable) {
        return -1;
    } else {
        return parent->get_variable_default_axiom_value(var);
    }
}

string MinimalReductionTask::get_fact_name(const FactPair &fact) const {
    if (fact.var == position_variable) {
        return "plan_position=" + to_string(fact.value);
    } else {
        return parent->get_fact_name(fact);
    }
}

bool MinimalReductionTask::are_facts_mutex(
    const FactPair &fact1, const FactPair &fact2) const {
    if (fact1.var == position_variable && fact2.var == position_variable) {
        return fact1.value != fact2.value;
    }

    if (fact1.var == position_variable || fact2.var == position_variable) {
        return false;
    }

    return parent->are_facts_mutex(fact1, fact2);
}

// Returns true for every second operator
bool MinimalReductionTask::is_skip_operator(int index) const {
    return index % 2 == 1;
}

int MinimalReductionTask::get_plan_position(int index) const {
    return index / 2;
}

// Here begins the harder part. We only need all operators in the plan and one
// skip operator operator per operator :)
int MinimalReductionTask::get_num_operators() const {
    return plan.size() * 2;
}

int MinimalReductionTask::get_operator_cost(int index, bool is_axiom) const {
    if (is_axiom) {
        return parent->get_operator_cost(index, true);
    }

    if (is_skip_operator(index)) {
        return 0;
    }

    int plan_position = get_plan_position(index);
    int original_operator_index = plan[plan_position].get_index();

    return parent->get_operator_cost(original_operator_index, false);
}

string MinimalReductionTask::get_operator_name(int index, bool is_axiom) const {
    if (is_axiom) {
        return parent->get_operator_name(index, true);
    }
    if (is_skip_operator(index)) {
        return "skip_" + to_string(get_plan_position(index));
    } else {
        int original_operator_index =
            plan[get_plan_position(index)].get_index();
        return parent->get_operator_name(original_operator_index, false);
    }
}

int MinimalReductionTask::get_num_operator_preconditions(
    int index, bool is_axiom) const {
    if (is_axiom) {
        return parent->get_num_operator_preconditions(index, true);
    }
    if (is_skip_operator(index)) {
        return 1;
    }

    int original_operator_index = plan[get_plan_position(index)].get_index();

    return parent->get_num_operator_preconditions(
               original_operator_index, false) +
           1;
}

FactPair MinimalReductionTask::get_operator_precondition(
    int op_index, int fact_index, bool is_axiom) const {
    if (is_axiom) {
        return parent->get_operator_precondition(op_index, fact_index, true);
    }

    int plan_position = get_plan_position(op_index);

    if (is_skip_operator(op_index)) {
        return FactPair(position_variable, plan_position);
    }

    int original_operator_index = plan[plan_position].get_index();
    int original_num_preconditions =
        parent->get_num_operator_preconditions(original_operator_index, false);

    if (fact_index < original_num_preconditions) {
        return parent->get_operator_precondition(
            original_operator_index, fact_index, false);
    }
    return FactPair(position_variable, plan_position);
}

int MinimalReductionTask::get_num_operator_effects(
    int index, bool is_axiom) const {
    if (is_axiom) {
        return parent->get_num_operator_effects(index, true);
    }
    if (is_skip_operator(index)) {
        return 1;
    }

    int original_operator_index = plan[get_plan_position(index)].get_index();

    return parent->get_num_operator_effects(original_operator_index, false) + 1;
}

FactPair MinimalReductionTask::get_operator_effect(
    int op_index, int fact_index, bool is_axiom) const {
    if (is_axiom) {
        return parent->get_operator_effect(op_index, fact_index, true);
    }

    int plan_position = get_plan_position(op_index);

    if (is_skip_operator(op_index)) {
        return FactPair(position_variable, plan_position + 1);
    }

    int original_operator_index = plan[plan_position].get_index();

    int original_num_effects =
        parent->get_num_operator_effects(original_operator_index, false);

    if (fact_index < original_num_effects) {
        return parent->get_operator_effect(
            original_operator_index, fact_index, false);
    }

    return FactPair(position_variable, plan_position + 1);
}

int MinimalReductionTask::get_num_operator_effect_conditions(
    int op_index, int effect_index, bool is_axiom) const {
    if (is_axiom) {
        return parent->get_num_operator_effect_conditions(
            op_index, effect_index, true);
    }

    if (is_skip_operator(op_index)) {
        return 0;
    }

    int original_operator_index = plan[get_plan_position(op_index)].get_index();

    int original_num_effects =
        parent->get_num_operator_effects(original_operator_index, false);

    if (effect_index < original_num_effects) {
        return parent->get_num_operator_effect_conditions(
            original_operator_index, effect_index, false);
    }

    // Added pos := i + 1 effect is unconditional.
    return 0;
}

FactPair MinimalReductionTask::get_operator_effect_condition(
    int op_index, int effect_index, int condition_index, bool is_axiom) const {
    if (is_axiom) {
        return parent->get_operator_effect_condition(
            op_index, effect_index, condition_index, true);
    }

    int original_operator_index = plan[get_plan_position(op_index)].get_index();

    return parent->get_operator_effect_condition(
        original_operator_index, effect_index, condition_index, false);
}

vector<int> MinimalReductionTask::get_initial_state_values() const {
    vector<int> values = parent->get_initial_state_values();

    values.push_back(0);

    return values;
}