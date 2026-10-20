#ifndef PLAN_NEIGHBORHOOD_GRAPH_SEARCH_H
#define PLAN_NEIGHBORHOOD_GRAPH_SEARCH_H

#include "plan_improver.h"

#include <cstddef>
#include <memory>

class AbstractTask;
class StateRegistry;

class PlanNeighborhoodGraphSearch : public PlanImprover {
    std::size_t neighborhood_size;

public:
    explicit PlanNeighborhoodGraphSearch(std::size_t neighborhood_size = 5);

    Plan improve(
        const Plan &plan, const std::shared_ptr<AbstractTask> &task,
        StateRegistry &state_registry) override;
};

#endif