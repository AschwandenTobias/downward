#include "action_elimination.h"

#include <iostream>
using namespace std;

Plan action_elimination(const Plan &plan, const TaskProxy &taskProxy) {
    cout << "action_elimination is called\n";
    State initialState = taskProxy.get_initial_state();
    for (int i = 0; i < plan.size(); i++) {
        cout << "Try to remove action and simulate rest of plan: " << i << "\n";
    }
    return plan;
}