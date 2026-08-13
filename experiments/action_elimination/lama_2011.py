#!/usr/bin/env python3

import os

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
    ENV = project.BaselSlurmEnvironment()

else:
    SUITE = [
        "blocks:probBLOCKS-4-0.pddl",
        "blocks:probBLOCKS-5-0.pddl",
        "blocks:probBLOCKS-16-2.pddl",
        "blocks:probBLOCKS-17-0.pddl",
        "blocks:probBLOCKS-15-0.pddl",
        "elevators-sat08-strips:p01.pddl",
        "elevators-sat08-strips:p02.pddl",
    ]

    ENV = project.LocalEnvironment(processes=12)


# ---------------------------------------------------------------------------
# Planner configurations
# ---------------------------------------------------------------------------

CONFIGS = [
    (
        "lama-2011",
        [],
        [
            "--alias",
            "seq-sat-lama-2011",
        ],
    ),
]


# ---------------------------------------------------------------------------
# Resource limits
# ---------------------------------------------------------------------------

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

    # Best plan found.
    "cost",

    # All improving plan costs found during the anytime search.
    "cost:all",

    # Resource usage.
    "planner_time",
    "planner_wall_clock_time",
    "planner_memory",
]


# ---------------------------------------------------------------------------
# Experiment
# ---------------------------------------------------------------------------

exp = project.FastDownwardExperiment(
    environment=ENV,
    revision_cache=REVISION_CACHE,
)


for config_nick, config, extra_driver_options in CONFIGS:
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
            driver_options=DRIVER_OPTIONS + extra_driver_options,
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

# LAMA 2011 is an iterated / anytime planner.
exp.add_parser(exp.ANYTIME_SEARCH_PARSER)

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