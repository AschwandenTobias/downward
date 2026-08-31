#!/usr/bin/env python3

import os

import custom_parser
import project

REPO = project.get_repo_base()
BENCHMARKS_DIR = os.environ["DOWNWARD_BENCHMARKS"]

REVISION_CACHE = (
    os.environ.get("DOWNWARD_REVISION_CACHE")
    or project.DIR / "data" / "revision-cache"
)


# ---------------------------------------------------------------------------
# Environment and benchmark suite
# ---------------------------------------------------------------------------

if project.REMOTE:
    # On sciCORE:
    # Run all satisficing benchmark domains through SLURM.
    SUITE = project.SUITE_SATISFICING
    ENV = project.BaselSlurmEnvironment()

else:
    # Local testing:
    # Only run a small subset to quickly test the experiment setup.
    # Quickly here still means at least 5 minutes since some of those are not solvable in that time.
    SUITE = [
        "blocks:probBLOCKS-4-0.pddl",
        "blocks:probBLOCKS-5-0.pddl",
        "elevators-sat08-strips:p01.pddl",
        "elevators-sat08-strips:p02.pddl",
    ]

    ENV = project.LocalEnvironment(processes=12)


# ---------------------------------------------------------------------------
# Planner configurations
# ---------------------------------------------------------------------------

SEARCH = "let(hff, ff(), lazy_greedy([hff], preferred=[hff]))"


CONFIGS = [
    # Baseline: no plan improvement.
    (
    "lama-first",
    [
        "--search",
        (
            "let(hlm, eval_modify_costs("
            "landmark_sum("
            "lm_factory=lm_reasonable_orders_hps(lm_rhw()),"
            "pref=false),"
            "cost_type=one),"
            "let(hff, eval_modify_costs(ff(),cost_type=one),"
            "lazy_greedy("
            "[hff,hlm],"
            "preferred=[hff,hlm],"
            "cost_type=one,"
            "reopen_closed=false"
            ")))"
        ),
    ],
),
    # Action elimination.
    (
    "lama-first-ae",
    [
        "--search",
        (
            "let(hlm, eval_modify_costs("
            "landmark_sum("
            "lm_factory=lm_reasonable_orders_hps(lm_rhw()),"
            "pref=false),"
            "cost_type=one),"
            "let(hff, eval_modify_costs(ff(),cost_type=one),"
            "lazy_greedy("
            "[hff,hlm],"
            "preferred=[hff,hlm],"
            "cost_type=one,"
            "reopen_closed=false"
            ")))"
        ),
        "--plan-improvement",
        "ae",
    ],
),

    # Action elimination using tracked plan states.
    (
    "lama-first-ae-plan-states",
    [
        "--search",
        (
            "let(hlm, eval_modify_costs("
            "landmark_sum("
            "lm_factory=lm_reasonable_orders_hps(lm_rhw()),"
            "pref=false),"
            "cost_type=one),"
            "let(hff, eval_modify_costs(ff(),cost_type=one),"
            "lazy_greedy("
            "[hff,hlm],"
            "preferred=[hff,hlm],"
            "cost_type=one,"
            "reopen_closed=false"
            ")))"
        ),
        "--plan-improvement",
        "ae_plan_states",
    ],
),
]

BUILD_OPTIONS = []

DRIVER_OPTIONS = [
    "--overall-time-limit",
    "5m",
    "--overall-memory-limit",
    "2G",
]

# ---------------------------------------------------------------------------
# Fast Downward revision
# ---------------------------------------------------------------------------

REV_NICKS = [
    ("operator_reduction", ""),
]

# ---------------------------------------------------------------------------
# Report attributes
# ---------------------------------------------------------------------------

ATTRIBUTES = [
    "algorithm",
    "domain",
    "problem",
    "coverage",
    "error",
    "cost",
    "plan_length",
    "number_of_operators",
    "number_of_reduced_operators",
    "allowed_operator_name_schemas",
    "graph_states",
    "graph_edges",
    "search_time",
    project.PLAN_IMPROVEMENT_TIME,
    "total_time",
    "memory",
]


# ---------------------------------------------------------------------------
# Experiment
# ---------------------------------------------------------------------------

exp = project.FastDownwardExperiment(
    environment=ENV,
    revision_cache=REVISION_CACHE,
)

for config_nick, config in CONFIGS:
    for revision, revision_nick in REV_NICKS:

        if revision_nick:
            algorithm_name = f"{revision_nick}:{config_nick}"
        else:
            algorithm_name = config_nick

        exp.add_algorithm(
            algorithm_name,
            REPO,
            revision,
            config,
            build_options=BUILD_OPTIONS,
            driver_options=DRIVER_OPTIONS,
        )

exp.add_suite(
    BENCHMARKS_DIR,
    SUITE,
)

# ---------------------------------------------------------------------------
# Parsers
# ---------------------------------------------------------------------------

exp.add_parser(exp.EXITCODE_PARSER)
exp.add_parser(exp.TRANSLATOR_PARSER)
exp.add_parser(exp.SINGLE_SEARCH_PARSER)

# Parse our custom "Plan improvement time" output.
exp.add_parser(custom_parser.get_parser())

exp.add_parser(exp.PLANNER_PARSER)


# ---------------------------------------------------------------------------
# Experiment steps
# ---------------------------------------------------------------------------

exp.add_step(
    "build",
    exp.build,
)

exp.add_step(
    "start",
    exp.start_runs,
)

exp.add_step(
    "parse",
    exp.parse,
)

exp.add_fetcher(
    name="fetch",
)


# ---------------------------------------------------------------------------
# Reports
# ---------------------------------------------------------------------------

project.add_absolute_report(
    exp,
    attributes=ATTRIBUTES,
)


# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------

exp.run_steps()