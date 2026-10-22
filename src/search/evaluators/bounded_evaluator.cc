#include "bounded_evaluator.h"

#include "../evaluation_context.h"
#include "../evaluation_result.h"

#include "../plugins/plugin.h"
#include "../utils/component_errors.h"

using namespace std;

namespace bounded_evaluator {

BoundedEvaluator::BoundedEvaluator(
    const shared_ptr<AbstractTask> &task, const shared_ptr<Evaluator> &eval,
    int bound, const string &description, utils::Verbosity verbosity)
    : Evaluator(task, false, false, false, description, verbosity),
      evaluator(eval),
      bound(bound) {
    utils::verify_argument(bound >= 0, "Bound must be non-negative.");
}

bool BoundedEvaluator::is_safe() const {
    return evaluator->is_safe();
}

EvaluationResult BoundedEvaluator::compute_result(
    EvaluationContext &eval_context) {
    EvaluationResult result;

    int value = eval_context.get_evaluator_value_or_infinity(evaluator.get());

    if (value != EvaluationResult::INFTY && value >= bound) {
        value = EvaluationResult::INFTY;
    }

    result.set_evaluator_value(value);
    return result;
}

void BoundedEvaluator::get_path_dependent_evaluators(set<Evaluator *> &evals) {
    evaluator->get_path_dependent_evaluators(evals);
}

class BoundedEvaluatorFeature
    : public plugins::TypedFeature<TaskIndependentEvaluator> {
public:
    BoundedEvaluatorFeature() : TypedFeature("bounded") {
        document_subcategory("evaluators_basic");
        document_title("Bounded evaluator");
        document_synopsis(
            "Returns infinity if the wrapped evaluator reaches "
            "or exceeds the given bound.");

        add_option<shared_ptr<TaskIndependentEvaluator>>("eval", "evaluator");

        add_option<int>(
            "bound", "exclusive evaluator bound", "infinity",
            plugins::Bounds("0", "infinity"));

        add_evaluator_options_to_feature(*this, "bounded");
    }

    shared_ptr<TaskIndependentEvaluator> create_component(
        const plugins::Options &opts) const override {
        return components::make_auto_task_independent_component<
            BoundedEvaluator, Evaluator>(
            opts.get<shared_ptr<TaskIndependentEvaluator>>("eval"),
            opts.get<int>("bound"), get_evaluator_arguments_from_options(opts));
    }
};

static plugins::FeaturePlugin<BoundedEvaluatorFeature> _plugin;

}