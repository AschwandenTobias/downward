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
    SUITE = project.SUITE_SATISFICING

    ENV = project.BaselSlurmEnvironment(
        partition="infai_2",
        memory_per_cpu="6300M",
        cpus_per_task=2,
        time_limit_per_task="01:10:00",
    )

else:
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

LAMA_FIRST = (
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
)


CONFIGS = [
    (
        "0_lf",
        [
            "--search",
            LAMA_FIRST,
        ],
    ),

    (
        "1_lf_ae",
        [
            "--search",
            LAMA_FIRST,
            "--plan-improvement",
            "ae",
        ],
    ),

    (
        "2_lf_mr",
        [
            "--search",
            LAMA_FIRST,
            "--plan-improvement",
            "minimal_reduction",
        ],
    ),

    (
        "3_lf_planStates",
        [
            "--search",
            LAMA_FIRST,
            "--plan-improvement",
            "plan_states",
        ],
    ),

    (
        "4_lf_combined_reductions",
        [
            "--search",
            LAMA_FIRST,
            "--plan-improvement",
            "combined_reductions",
        ],
    ),

    (
        "4b_lf_combined_non_static",
        [
            "--search",
            LAMA_FIRST,
            "--plan-improvement",
            "combined_reductions_greedy",
        ],
    ),

    (
        "5_lf_operator_reduction",
        [
            "--search",
            LAMA_FIRST,
            "--plan-improvement",
            "operator_reduction",
        ],
    ),

    (
        "6_lf_operator_name_reduction",
        [
            "--search",
            LAMA_FIRST,
            "--plan-improvement",
            "operator_name_reduction",
        ],
    ),

    (
        "7_optimal_hmax",
        [
            "--search",
            "astar(hmax())",
        ],
    ),
]


BUILD_OPTIONS = []


DRIVER_OPTIONS = [
    "--overall-time-limit",
    "1h",
    "--overall-memory-limit",
    "12G",
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