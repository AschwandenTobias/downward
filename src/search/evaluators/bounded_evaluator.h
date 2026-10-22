#ifndef EVALUATORS_BOUNDED_EVALUATOR_H
#define EVALUATORS_BOUNDED_EVALUATOR_H

#include "../evaluator.h"

#include <memory>

namespace bounded_evaluator {

class BoundedEvaluator : public Evaluator {
    std::shared_ptr<Evaluator> evaluator;
    int bound;

public:
    BoundedEvaluator(
        const std::shared_ptr<AbstractTask> &task,
        const std::shared_ptr<Evaluator> &eval, int bound,
        const std::string &description, utils::Verbosity verbosity);

    bool is_safe() const override;

    EvaluationResult compute_result(EvaluationContext &eval_context) override;

    void get_path_dependent_evaluators(std::set<Evaluator *> &evals) override;
};

}

#endif