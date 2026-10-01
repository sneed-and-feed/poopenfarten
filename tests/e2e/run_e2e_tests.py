"""
PPF-42 DYNAMICS (Poopenfarten Pro)
Automated Headless E2E Test Suite Runner
Authoritative Reference: PROJECT.md, PPF42_DYNAMICS_SPEC.md, spec_inventory.md, TEST_INFRA.md
"""

import sys
import os
import argparse
import time
import json
from typing import Dict, List, Any

# Ensure tests/e2e is on python path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from test_framework import TestRunnerContext, TestResult
from tier1_feature_tests import run_tier1_tests
from tier2_boundary_tests import run_tier2_tests
from tier3_interaction_tests import run_tier3_tests
from tier4_production_tests import run_tier4_tests
from tier5_adversarial_tests import run_tier5_tests


def main():
    parser = argparse.ArgumentParser(description="PPF-42 DYNAMICS Automated E2E Test Runner")
    parser.add_argument("--tier", type=int, choices=[1, 2, 3, 4, 5], help="Run only tests from specified tier")
    parser.add_argument("--feature", type=str, help="Run only tests matching feature ID (e.g., F01)")
    parser.add_argument("--verbose", action="store_true", help="Print detailed diagnostic output for every test")
    parser.add_argument("--json-out", type=str, help="Save structured test execution summary to JSON file")
    args = parser.parse_args()

    print("=" * 80)
    print("      PPF-42 DYNAMICS (POOPENFARTEN PRO) — AUTOMATED E2E TEST RUNNER")
    print("=" * 80)
    print(f"Target Subsystems : DSP Core, APVTS Layout, Presets, Web Telemetry, Voicing")
    print(f"Test Philosophy   : Opaque-Box, Requirement-Driven, Zero-Coupling")
    print(f"Standard Ref      : TEST_INFRA.md, PPF42_DYNAMICS_SPEC.md, PROJECT.md")
    print("-" * 80)

    ctx = TestRunnerContext()
    ctx.start_time = time.perf_counter()

    # Determine which tiers to run
    run_t1 = args.tier is None or args.tier == 1
    run_t2 = args.tier is None or args.tier == 2
    run_t3 = args.tier is None or args.tier == 3
    run_t4 = args.tier is None or args.tier == 4
    run_t5 = args.tier is None or args.tier == 5

    if run_t1:
        print("[>] Executing Tier 1: Feature Coverage (Category-Partition)...")
        run_tier1_tests(ctx)
    if run_t2:
        print("[>] Executing Tier 2: Boundary & Corner Cases (Extreme Numerics)...")
        run_tier2_tests(ctx)
    if run_t3:
        print("[>] Executing Tier 3: Cross-Feature Pairwise Interactions...")
        run_tier3_tests(ctx)
    if run_t4:
        print("[>] Executing Tier 4: Real-World Application Workloads...")
        run_tier4_tests(ctx)
    if run_t5:
        print("[>] Executing Tier 5: Adversarial Coverage Hardening...")
        run_tier5_tests(ctx)

    total_duration_sec = time.perf_counter() - ctx.start_time

    # Filter by feature if requested
    results_to_report = ctx.results
    if args.feature:
        feat_query = args.feature.upper()
        results_to_report = [r for r in ctx.results if feat_query in r.feature_id.upper()]

    # Print verbose results if requested
    if args.verbose:
        print("\n--- Detailed Test Diagnostics ---")
        for r in results_to_report:
            status = "[PASS]" if r.passed else "[FAIL]"
            print(f"  {status} [{r.test_id}] (Tier {r.tier}, {r.feature_id}) {r.name} - {r.duration_ms:.2f}ms")
            if not r.passed:
                print(f"        ERROR: {r.message}")
            if r.diagnostics:
                print(f"        DIAG : {r.diagnostics}")

    # Aggregations
    tier_counts = {1: {"total": 0, "pass": 0, "fail": 0},
                   2: {"total": 0, "pass": 0, "fail": 0},
                   3: {"total": 0, "pass": 0, "fail": 0},
                   4: {"total": 0, "pass": 0, "fail": 0},
                   5: {"total": 0, "pass": 0, "fail": 0}}

    feature_counts: Dict[str, Dict[str, int]] = {}

    for r in results_to_report:
        t = r.tier
        if t in tier_counts:
            tier_counts[t]["total"] += 1
            if r.passed:
                tier_counts[t]["pass"] += 1
            else:
                tier_counts[t]["fail"] += 1

        f_id = r.feature_id
        if f_id not in feature_counts:
            feature_counts[f_id] = {"total": 0, "pass": 0, "fail": 0}
        feature_counts[f_id]["total"] += 1
        if r.passed:
            feature_counts[f_id]["pass"] += 1
        else:
            feature_counts[f_id]["fail"] += 1

    total_tests = len(results_to_report)
    passed_tests = sum(1 for r in results_to_report if r.passed)
    failed_tests = total_tests - passed_tests
    pass_rate = (passed_tests / total_tests * 100.0) if total_tests > 0 else 0.0

    print("\n" + "=" * 80)
    print("                         TEST EXECUTION SUMMARY")
    print("=" * 80)
    print(f"{'Tier':<8} | {'Description':<40} | {'Total':<8} | {'Passed':<8} | {'Failed':<8} | {'Rate':<8}")
    print("-" * 86)
    tier_names = {
        1: "Tier 1: Feature Coverage (Category-Partition)",
        2: "Tier 2: Boundary & Corner Cases",
        3: "Tier 3: Cross-Feature Interactions",
        4: "Tier 4: Real-World Application Scenarios",
        5: "Tier 5: Adversarial Coverage Hardening"
    }
    for t_num in sorted(tier_counts.keys()):
        cnt = tier_counts[t_num]
        if cnt["total"] > 0:
            rate = f"{(cnt['pass']/cnt['total']*100.0):.1f}%"
            print(f"Tier {t_num:<3} | {tier_names[t_num]:<40} | {cnt['total']:<8} | {cnt['pass']:<8} | {cnt['fail']:<8} | {rate:<8}")

    print("-" * 86)
    print(f"{'TOTAL':<8} | {'All Tiers Combined':<40} | {total_tests:<8} | {passed_tests:<8} | {failed_tests:<8} | {pass_rate:.1f}%")
    print("=" * 86)

    # Feature breakdown table
    print("\n" + "-" * 80)
    print("                   FEATURE INVENTORY COVERAGE BREAKDOWN")
    print("-" * 80)
    print(f"{'Feature ID':<16} | {'Total Tests':<12} | {'Passed':<12} | {'Failed':<12} | {'Status':<12}")
    print("-" * 80)
    for fid in sorted(feature_counts.keys()):
        fc = feature_counts[fid]
        status = "PASS" if fc["fail"] == 0 else "FAIL"
        print(f"{fid:<16} | {fc['total']:<12} | {fc['pass']:<12} | {fc['fail']:<12} | {status:<12}")
    print("-" * 80)

    print(f"\nElapsed Execution Time: {total_duration_sec:.3f} seconds ({total_duration_sec*1000.0:.1f} ms)")

    # JSON export
    if args.json_out:
        summary_data = {
            "project": "PPF-42 DYNAMICS",
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "total_tests": total_tests,
            "passed_tests": passed_tests,
            "failed_tests": failed_tests,
            "pass_rate_pct": pass_rate,
            "duration_sec": total_duration_sec,
            "tier_summary": tier_counts,
            "feature_summary": feature_counts,
            "tests": [
                {
                    "test_id": r.test_id,
                    "feature_id": r.feature_id,
                    "tier": r.tier,
                    "name": r.name,
                    "passed": r.passed,
                    "message": r.message,
                    "duration_ms": r.duration_ms,
                    "diagnostics": r.diagnostics
                }
                for r in results_to_report
            ]
        }
        with open(args.json_out, "w", encoding="utf-8") as f:
            json.dump(summary_data, f, indent=2)
        print(f"[+] Saved structured summary to: {args.json_out}")

    if failed_tests > 0:
        print("\n[!] VERDICT: FAILED (One or more assertions did not pass).")
        sys.exit(1)
    else:
        print("\n[v] VERDICT: 100% PASS — ALL REQUIREMENTS & BOUNDARIES VERIFIED.")
        sys.exit(0)


if __name__ == "__main__":
    main()
