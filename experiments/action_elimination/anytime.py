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
        cpus_per_task=1,
        time_limit_per_task="00:35:00",
    )

else:
    SUITE = [
        "blocks:probBLOCKS-4-0.pddl",
        "blocks:probBLOCKS-5-0.pddl",
        "blocks:probBLOCKS-15-0.pddl",
        "elevators-sat08-strips:p01.pddl",
        "elevators-sat08-strips:p02.pddl",
        "elevators-sat08-strips:p23.pddl"
    ]

    ENV = project.LocalEnvironment(processes=12)


# ---------------------------------------------------------------------------
# Planner configuration
# ---------------------------------------------------------------------------

ITERATED_WASTAR_FF = (
    "iterated(["
    "lazy_wastar([ff()],w=5),"
    "lazy_wastar([ff()],w=3),"
    "lazy_wastar([ff()],w=2),"
    "lazy_wastar([ff()],w=1)"
    "],"
    "pass_bound=true,"
    "repeat_last=false,"
    "continue_on_solve=true,"
    "cost_type=normal,"
    "bound=infinity,"
    "max_time=infinity,"
    "description=\"iterated wastar ff\","
    "verbosity=normal"
    ")"
)


CONFIGS = [
    (
        "0_iterated_wastar_ff",
        [
            "--search",
            ITERATED_WASTAR_FF,
        ],
    ),
]


# ---------------------------------------------------------------------------
# Build / resource limits
# ---------------------------------------------------------------------------

BUILD_OPTIONS = []


DRIVER_OPTIONS = [
    "--overall-time-limit",
    "30s",
    "--overall-memory-limit",
    "1G",
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
    "search_time",
    "total_time",
    "memory",
    "first_plan_cost",
    "final_plan_cost",
    "num_plans",
    "anytime_plan_costs",
    "anytime_plan_times",
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
exp.add_parser(exp.ANYTIME_SEARCH_PARSER)

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