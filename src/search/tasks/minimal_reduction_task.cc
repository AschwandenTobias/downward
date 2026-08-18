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
        return -1;
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
    if (fact1.var == position_variable || fact2.var == position_variable) {
        return false;
    } else {
        return parent->are_facts_mutex(fact1, fact2);
    }
}