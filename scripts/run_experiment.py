#!/usr/bin/env python3

import argparse
import json
import os
import re
import sys
import subprocess
import shlex
import time
import statistics
from dataclasses import dataclass, asdict
from typing import List, Optional, Tuple
from pathlib import Path

SLEEP_BETWEEN_REPS_SEC = 30
STOPWATCH_FILENAME = ".experiment.json"
AGG_OUTPUT_FILENAME = "output.json"


def parse_range(arg: str) -> List[int]:
    """
    Parse comma-separated values and ranges like '20,22-24,30' into a list of ints.
    """
    if not arg:
        return []
    values: List[int] = []
    parts = [p.strip() for p in arg.split(",") if p.strip()]
    range_pattern = re.compile(r"^(\d+)-(\d+)$")
    single_pattern = re.compile(r"^\d+$")
    for part in parts:
        m = range_pattern.match(part)
        if m:
            start = int(m.group(1))
            end = int(m.group(2))
            if end < start:
                raise argparse.ArgumentTypeError(f"Invalid range '{part}': end < start")
            values.extend(list(range(start, end + 1)))
            continue
        if single_pattern.match(part):
            values.append(int(part))
            continue
        raise argparse.ArgumentTypeError(f"Invalid range token: '{part}'")
    return values

def parse_input_sizes(arg: str) -> List[int]:
    """
    Parse comma-separated input sizes where each token is either:
      - an exact integer: '1000000'
      - an expression a^b with integers a,b: '2^20', '10^6'
    Ranges are not supported in this mode.
    """
    if not arg:
        return []
    values: List[int] = []
    parts = [p.strip() for p in arg.split(",") if p.strip()]
    int_pattern = re.compile(r"^\d+$")
    pow_pattern = re.compile(r"^(\d+)\^(\d+)$")
    for part in parts:
        if int_pattern.match(part):
            values.append(int(part))
            continue
        m = pow_pattern.match(part)
        if m:
            base = int(m.group(1))
            exp = int(m.group(2))
            try:
                values.append(base ** exp)
            except OverflowError:
                raise argparse.ArgumentTypeError(f"Value too large in token '{part}'")
            continue
        raise argparse.ArgumentTypeError(f"Invalid input size token: '{part}'. Expected INT or a^b.")
    return values


def parse_thread_pow_range(arg: str) -> Tuple[int, int]:
    """
    Parse 'min[-max]' integer pattern into (min, max) powers of two.
    """
    m = re.match(r"^(\d+)(?:-(\d+))?$", arg)
    if not m:
        raise argparse.ArgumentTypeError("Invalid threads power range, expected MIN or MIN-MAX")
    min_pow = int(m.group(1))
    max_pow = int(m.group(2)) if m.group(2) is not None else min_pow
    if max_pow < min_pow:
        raise argparse.ArgumentTypeError("Invalid threads power range: max < min")
    return min_pow, max_pow


@dataclass
class ExperimentConfig:
    # Direct flags / inputs
    exp_protocol: int
    exp_setting: str
    exp_communicator: str
    specified_communicator: bool
    num_comm_threads: int
    row_exponents: List[int]
    use_power_of_10: bool
    scale_factor: Optional[float]
    use_scale_factor: bool
    num_parties: int
    min_threads_pow: int
    max_threads_pow: int
    threads: int
    exp_repetitions: int
    opt_level: int
    triples_type: str
    cmake_args: List[str]
    exp_args: List[str]
    node_prefix: str
    batch_size: int
    exp_name: str
    # Derived values
    exp_input_sizes: List[float]
    comm_cmake_arg: str
    iface: Optional[str]
    subnet: Optional[str]
    run_cmd: str
    num_process_flag: str
    host_flag: str
    exp_cmd_prefix: str
    build_dir: str
    results_dir: str
    git_commit: Optional[str]
    git_branch: Optional[str]
    git_status_porcelain: Optional[str]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog=os.path.basename(sys.argv[0]),
        description="Run an experiment with configurable protocol, setting, communicator, and sizes.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        "exp_name",
        help="Experiment to run",
    )
    parser.add_argument(
        "-p",
        dest="exp_protocol",
        type=int,
        default=3,
        choices=[0, 1, 2, 3, 4, 5],
        help="Protocol; default: 3",
    )
    parser.add_argument(
        "-npc",
        dest="num_parties",
        type=int,
        default=2,
        help="Number of parties; default: protocol number",
    )
    parser.add_argument(
        "-s",
        dest="exp_setting",
        choices=["same", "lan", "wan"],
        default="same",
        help="Setting; default: same",
    )
    parser.add_argument(
        "-c",
        dest="exp_communicator",
        choices=["mpi", "nocopy"],
        default=None,  # None so we can apply the 'same' => default 'mpi' rule
        help="Communicator; default: mpi for 'same', nocopy otherwise",
    )
    parser.add_argument(
        "-n",
        dest="num_comm_threads",
        type=int,
        default=-1,
        help="[NoCopyComm only] Number of communicator threads (negative: # per worker); default: -1",
    )
    parser.add_argument(
        "-r",
        dest="row_exponents_raw",
        default="2^20",
        help="Number of rows. Comma-separated tokens where each token is an INT or a^b; default: 2^20",
    )
    parser.add_argument(
        "-f",
        dest="scale_factor",
        type=float,
        default=None,
        help="Scale factor for TPC-H and other queries. Overrides -r if set.",
    )
    parser.add_argument(
        "-t",
        dest="threads_pow_raw",
        default=None,
        help="Number of threads, as powers of 2 (MIN or MIN-MAX)",
    )
    parser.add_argument(
        "-T",
        dest="threads",
        type=int,
        default=0,
        help="Number of threads (arbitrary); default: 0",
    )
    parser.add_argument(
        "-e",
        dest="exp_repetitions",
        type=int,
        default=1,
        help="Number of times to repeat each rows/threads pairing; default: 1",
    )
    parser.add_argument(
        "-m",
        dest="cmake_args",
        action="append",
        default=[],
        help="Pass additional arguments to cmake (can be repeated for more)",
    )
    parser.add_argument(
        "-a",
        dest="exp_args",
        action="append",
        default=[],
        help="Pass additional arguments to the experiment binary (can be repeated for more)",
    )
    parser.add_argument(
        "-x",
        dest="node_prefix",
        default="node",
        help="Prefix for remote nodes. Machines are prefix0, prefix1, ...; default: node",
    )
    parser.add_argument(
        "-b",
        dest="batch_size",
        type=int,
        default=-12,
        help="Batch size; default: -12",
    )
    parser.add_argument(
        "-o",
        dest="opt_level",
        type=int,
        default=2,
        choices=[0, 1, 2, 3],
        help="Optimization level; default: 2",
    )
    parser.add_argument(
        "-y",
        dest="triples_type",
        choices=["zero", "dummy", "real", "ZERO", "DUMMY", "REAL"],
        default="zero",
        help="Type of Beaver triples for 2PC (ZERO/DUMMY/REAL); default: zero",
    )
    return parser

def _run_cmd(cmd: List[str]) -> str:
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        return ""
    return res.stdout.strip()

def _get_git_info() -> Tuple[Optional[str], Optional[str], Optional[str]]:
    """
    Return (short_commit, branch, status_porcelain). Empty strings if unavailable.
    """
    commit = _run_cmd(["git", "rev-parse", "--short", "HEAD"])
    branch = _run_cmd(["git", "rev-parse", "--abbrev-ref", "HEAD"])
    status = _run_cmd(["git", "status", "--porcelain"])
    # Normalize to None when not available; keep empty string for clean status
    commit_val: Optional[str] = commit if commit else None
    branch_val: Optional[str] = branch if branch else None
    status_val: Optional[str] = status if status is not None else None
    return commit_val, branch_val, status_val

def _extract_iface_from_ip_route(route_output: str) -> Optional[str]:
    # Try to find 'dev <iface>' token
    m = re.search(r"\bdev\s+(\S+)", route_output)
    return m.group(1) if m else None

def discover_iface_and_subnet(node_prefix: str) -> Tuple[Optional[str], Optional[str]]:
    r"""
    Replicate the bash logic:
      _node=${node_prefix}1
      _iface=$(ip route get $(dig +short ${_node} | tail -n 1) | grep -Po "((?<=dev )\S*)")
      SUBNET=$(ip -o -f inet addr show ${iface} | awk '{print $4}')
    """
    node = f"{node_prefix}1"
    ip_candidate = _run_cmd(["bash", "-lc", f"dig +short {node} | tail -n 1"])
    if not ip_candidate:
        return None, None
    route_out = _run_cmd(["ip", "route", "get", ip_candidate])
    iface = _extract_iface_from_ip_route(route_out) if route_out else None
    if not iface:
        return None, None
    subnet = _run_cmd(["bash", "-lc", f"ip -o -f inet addr show {iface} | awk '{{print $4}}'"])
    subnet = subnet if subnet else None
    return iface, subnet


def derive_config(args: argparse.Namespace) -> ExperimentConfig:
    # Validate scale factor
    use_scale_factor = args.scale_factor is not None
    if use_scale_factor and not (args.scale_factor > 0):
        raise SystemExit("Error: Scale factor must be greater than zero.")

    # Validate repetitions
    if args.exp_repetitions < 1:
        raise SystemExit("Invalid repetitions; must be >= 1")

    # Default logic: use protocol number
    num_parties = args.exp_protocol
    
    # Parse input sizes from -r (a or a^b, comma-separated); only used when no scale factor
    row_exponents: List[int] = []
    exp_input_sizes: List[float] = []
    if not use_scale_factor:
        try:
            parsed_sizes = parse_input_sizes(args.row_exponents_raw)
        except argparse.ArgumentTypeError as e:
            raise SystemExit(str(e))
        if not parsed_sizes:
            # Maintain a sensible default if user passed an empty string
            parsed_sizes = parse_input_sizes("2^20")
        exp_input_sizes = [float(v) for v in parsed_sizes]

    # Parse threads power range
    if args.threads_pow_raw is not None:
        min_threads_pow, max_threads_pow = parse_thread_pow_range(args.threads_pow_raw)
    else:
        min_threads_pow, max_threads_pow = 0, 0

    # Determine communicator defaulting rule
    specified_communicator = args.exp_communicator is not None
    if specified_communicator:
        exp_communicator = args.exp_communicator
    else:
        exp_communicator = "mpi" if args.exp_setting == "same" else "nocopy"

    # Derived inputs list
    if use_scale_factor:
        exp_input_sizes = [float(args.scale_factor)]

    # Map communicator to cmake arg
    comm_cmake_arg = "MPI" if exp_communicator == "mpi" else "NOCOPY"

    # Network interface & subnet (only for non-'same' settings)
    iface: Optional[str] = None
    subnet: Optional[str] = None
    if args.exp_setting != "same":
        iface, subnet = discover_iface_and_subnet(args.node_prefix)
        if iface and subnet:
            print(f"Common interface: {iface}; subnet {subnet}")

    # Set communicator commands
    if exp_communicator == "mpi":
        if args.exp_setting == "same":
            run_cmd = "mpirun"
        else:
            # SUBNET may be None if discovery failed; keep placeholders empty in that case
            subnet_str = subnet or ""
            run_cmd = f"mpirun --mca btl_tcp_if_include {subnet_str} --mca oob_tcp_if_include {subnet_str}"
        num_process_flag = "-n"
        host_flag = "--host"
    elif exp_communicator == "nocopy":
        run_cmd = "startmpc"
        num_process_flag = "-n"
        host_flag = "-h"
    else:
        raise SystemExit(f"ERROR: Unsupported communicator: {exp_communicator}")

    # Git info (best-effort; do not fail if not a git repo)
    git_commit, git_branch, git_status_porcelain = _get_git_info()

    # Normalize triples type to uppercase
    triples_type = args.triples_type.upper()

    return ExperimentConfig(
        exp_protocol=args.exp_protocol,
        exp_setting=args.exp_setting,
        exp_communicator=exp_communicator,
        specified_communicator=specified_communicator,
        num_comm_threads=args.num_comm_threads,
        row_exponents=row_exponents,
        scale_factor=args.scale_factor if use_scale_factor else None,
        use_scale_factor=use_scale_factor,
        use_power_of_10=False,
        num_parties=num_parties,
        min_threads_pow=min_threads_pow,
        max_threads_pow=max_threads_pow,
        threads=args.threads,
        exp_repetitions=args.exp_repetitions,
        opt_level=args.opt_level,
        triples_type=triples_type,
        cmake_args=args.cmake_args or [],
        exp_args=args.exp_args or [],
        node_prefix=args.node_prefix,
        batch_size=args.batch_size,
        exp_name=args.exp_name,
        exp_input_sizes=exp_input_sizes,
        comm_cmake_arg=comm_cmake_arg,
        iface=iface,
        subnet=subnet,
        run_cmd=run_cmd,
        num_process_flag=num_process_flag,
        host_flag=host_flag,
        exp_cmd_prefix="",
        build_dir="",
        results_dir="",
        git_commit=git_commit,
        git_branch=git_branch,
        git_status_porcelain=git_status_porcelain,
    )


def _flatten_args(args_list: List[str]) -> List[str]:
    """
    Flatten a list of possibly space-containing args using shlex.split for each item.
    """
    flat: List[str] = []
    for item in args_list:
        if not item:
            continue
        flat.extend(shlex.split(item))
    return flat


def perform_experiment_setup(cfg: ExperimentConfig) -> None:
    """
    Experiment setup:
     - Build experiment in ../build
     - Append COMM_THREADS for nocopy
     - Run cmake and make
     - scp binary to other nodes if needed
     - Prepare results directory
     - Generate EXP_CMD_PREFIX
    """
    cwd = os.getcwd()
    build_dir = os.path.abspath(os.path.join(cwd, "..", "build"))
    os.makedirs(build_dir, exist_ok=True)

    # Extra cmake args
    extra_cmake_args = list(cfg.cmake_args)
    if cfg.exp_communicator == "nocopy":
        extra_cmake_args = extra_cmake_args + [f"-DCOMM_THREADS={cfg.num_comm_threads}"]
    # Add triples type to cmake args
    extra_cmake_args = extra_cmake_args + [f"-DTRIPLES={cfg.triples_type}"]
    cmake_args_flat = _flatten_args(extra_cmake_args)

    # Run cmake
    cmake_cmd = ["cmake", "..", f"-DPROTOCOL={cfg.exp_protocol}", f"-DCOMM={cfg.comm_cmake_arg}", f"-DOPT_LEVEL={cfg.opt_level}"] + cmake_args_flat
    res = subprocess.run(cmake_cmd, cwd=build_dir)
    if res.returncode != 0:
        raise SystemExit(res.returncode)

    # Run make
    make_cmd = ["make", "-j", cfg.exp_name]
    res = subprocess.run(make_cmd, cwd=build_dir)
    if res.returncode != 0:
        raise SystemExit(res.returncode)

    # Send binary to other machines if not 'same'
    if cfg.exp_setting != "same":
        local_bin = os.path.join(build_dir, cfg.exp_name)
        remote_dir = os.path.join(cwd, "..", "build")
        for i in range(1, cfg.num_parties):
            host = f"{cfg.node_prefix}{i}"
            dest = f"{host}:{remote_dir}"
            res = subprocess.run(["scp", "-o", "StrictHostKeyChecking=no", local_bin, dest])
            if res.returncode != 0:
                raise SystemExit(res.returncode)

    # Prepare output directory (single shared results directory; no per-setting subdir)
    results_dir = os.path.abspath(os.path.join(cwd, ".", "results"))
    os.makedirs(results_dir, exist_ok=True)

    # Generate command prefix
    exp_cmd_prefix = ""
    if cfg.num_parties > 1:
        exp_cmd_prefix = f"{cfg.run_cmd} {cfg.num_process_flag} {cfg.num_parties}"
        if cfg.exp_setting != "same":
            # host list node0,...,node{num_parties-1}
            hosts = ",".join([f"{cfg.node_prefix}{i}" for i in range(0, cfg.num_parties)])
            exp_cmd_prefix = f"{exp_cmd_prefix} {cfg.host_flag} {hosts}"
    else:
        if cfg.exp_setting != "same":
            print("WARNING: settings other than 'same' have no effect for 1pc", file=sys.stderr)

    # Update cfg with derived paths and prefix
    cfg.build_dir = build_dir
    cfg.results_dir = results_dir
    cfg.exp_cmd_prefix = exp_cmd_prefix


def run_experiments(cfg: ExperimentConfig) -> None:
    """
    Execute the experiment runs across input sizes and thread configurations.
    Mirrors the bash run loop, without output handling/aggregation.
    """
    exp_args_flat = _flatten_args(cfg.exp_args)
    prefix_parts = shlex.split(cfg.exp_cmd_prefix) if cfg.exp_cmd_prefix else []
    experiment_records: List[dict] = []

    for input_size in cfg.exp_input_sizes:
        if cfg.use_scale_factor:
            exp_input = cfg.scale_factor
            input_suffix = "SF"
        else:
            # Use exact integer sizes derived from -r
            exp_input = int(input_size)
            input_suffix = "rows"

        # Build base flagged command for the binary
        base_cmd = [
            f"./{cfg.exp_name}",
            "-b", str(cfg.batch_size),
            "-r", str(exp_input),
            "-s", cfg.exp_setting,
            "-x", cfg.node_prefix,
        ] + exp_args_flat

        # Build list of thread counts to run (powers of two when threads==0)
        threads_to_run = [1 << e for e in range(cfg.min_threads_pow, cfg.max_threads_pow + 1)] if cfg.threads == 0 else [cfg.threads]

        for threads in threads_to_run:
            print(f"==== {exp_input} {input_suffix}; {threads} threads ====", flush=True)
            cmd_parts = base_cmd + ["-t", str(threads)]
            full_cmd = (prefix_parts + cmd_parts) if prefix_parts else cmd_parts
            for rep in range(cfg.exp_repetitions):
                _clear_stopwatch_file(cfg.build_dir)
                res = subprocess.run(full_cmd, cwd=cfg.build_dir)
                if res.returncode != 0:
                    print("Returned non-zero exit code", res.returncode)

                stopwatch_obj = _read_stopwatch_file(cfg.build_dir)
                run_cfg = _select_run_config(
                    cfg=cfg,
                    exp_input=exp_input,
                    input_suffix=input_suffix,
                    threads=threads,
                    repetition_index=rep,
                    cmd=full_cmd,
                )
                experiment_records.append({"config": run_cfg, "stopwatch": stopwatch_obj})
                if cfg.exp_communicator == "nocopy" and rep + 1 < cfg.exp_repetitions:
                    # sleep to prevent socket issues with nocopy
                    time.sleep(SLEEP_BETWEEN_REPS_SEC)
                time.sleep(1)

    grouped = _group_experiment_records(experiment_records)
    _write_experiment_json(cfg.build_dir, grouped)


def _select_run_config(
    cfg: ExperimentConfig,
    exp_input: float,
    input_suffix: str,
    threads: int,
    repetition_index: int,
    cmd: List[str],
) -> dict:
    return {
        "exp_name": cfg.exp_name,
        "protocol": cfg.exp_protocol,
        "num_parties": cfg.num_parties,
        "setting": cfg.exp_setting,
        "communicator": cfg.exp_communicator,
        "comm_cmake_arg": cfg.comm_cmake_arg,
        "triples_type": cfg.triples_type,
        "batch_size": cfg.batch_size,
        "input_size": exp_input,
        "threads": threads,
        "repetition_index": repetition_index,
        "node_prefix": cfg.node_prefix,
        "num_comm_threads": cfg.num_comm_threads,
        "scale_factor": cfg.scale_factor if cfg.use_scale_factor else None,
        "cmake_args": _flatten_args(cfg.cmake_args),
        "exp_args": _flatten_args(cfg.exp_args),
        "iface": cfg.iface,
        "subnet": cfg.subnet,
        "command": cmd,
        "git_commit": cfg.git_commit,
        "git_branch": cfg.git_branch,
        "git_status_porcelain": cfg.git_status_porcelain,
    }


def _clear_stopwatch_file(dir_path: str) -> None:
    try:
        os.remove(os.path.join(dir_path, STOPWATCH_FILENAME))
    except FileNotFoundError:
        pass


def _read_stopwatch_file(dir_path: str) -> Optional[object]:
    """
    Read stopwatch JSON written by the experiment binary (expected at .experiment.json).
    Returns the parsed JSON, or None if missing or invalid.
    """
    path = os.path.join(dir_path, STOPWATCH_FILENAME)
    try:
        with open(path, "r", encoding="utf-8") as f:
            text = f.read()
        # First try a normal parse
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            # Attempt recovery for known issue: missing final closing brace
            stripped = text.rstrip()
            # Balance braces if off by one
            open_count = stripped.count("{")
            close_count = stripped.count("}")
            if open_count > close_count:
                repaired = stripped + ("}" * (open_count - close_count))
                try:
                    return json.loads(repaired)
                except json.JSONDecodeError:
                    return None
            return None
    except FileNotFoundError:
        return None


def _write_experiment_json(output_dir: str, records: List[dict]) -> None:
    """
    Atomically write experiment records to build/output.json.
    Pretty-print each record (multi-line JSON) one after another, separated by a newline.
    No enclosing array.
    """
    out_path = os.path.join(output_dir, AGG_OUTPUT_FILENAME)
    tmp_path = out_path + ".tmp"
    with open(tmp_path, "w", encoding="utf-8") as f:
        for rec in records:
            f.write(json.dumps(rec, indent=2))
            f.write("\n")
    os.replace(tmp_path, out_path)


def _select_group_config(run_cfg: dict) -> dict:
    """
    Produce the grouping configuration by removing per-run fields.
    """
    group_cfg = dict(run_cfg)
    # Remove fields that vary per repetition
    group_cfg.pop("repetition_index", None)
    group_cfg.pop("command", None)
    return group_cfg


def _aggregate_profiles(stopwatches: List[dict]) -> dict:
    """
    Aggregate 'profile' metrics across a list of stopwatch dicts.
    Returns a dict like: { "profile": { label: {avg, min, max, count} } }
    If no stopwatch has a 'profile' dict, returns {}.
    """
    # Filter out None and non-dicts
    valid = [sw for sw in stopwatches if isinstance(sw, dict)]
    if not valid:
        return {}
    # Collect per-label values
    label_to_values: dict = {}
    for sw in valid:
        profile = sw.get("profile")
        # Prefer nested 'profile' dict when present; otherwise treat the stopwatch
        # itself as a flat mapping from label -> numeric value.
        items = profile.items() if isinstance(profile, dict) else sw.items()
        for label, value in items:
            # Only consider string labels with numeric-convertible values
            if not isinstance(label, str):
                continue
            try:
                v = float(value)
            except (TypeError, ValueError):
                continue
            label_to_values.setdefault(label, []).append(v)
    if not label_to_values:
        return {}
    aggregated_profile: dict = {}
    for label, values in label_to_values.items():
        if not values:
            continue
        vmin = min(values)
        vmax = max(values)
        vmean = round(sum(values) / len(values), 6)
        vmedian = round(statistics.median(values), 6)
        vstdev = round(statistics.stdev(values), 6) if len(values) >= 2 else None
        aggregated_profile[label] = {
            "mean": vmean,
            "min": vmin,
            "max": vmax,
            "median": vmedian,
            "stdev": vstdev,
            "count": len(values),
        }
    return aggregated_profile


def _group_experiment_records(records: List[dict]) -> List[dict]:
    """
    Group per-run records by configuration and compute aggregated stats.
    Output shape per group:
      {
        "config": { ...group config... },
        "raw_data": [ ...stopwatch objects per run (may include null)... ],
        "aggregated_data": { "profile": { label: {avg, min, max, count} } }
      }
    """
    groups: dict = {}
    order: List[str] = []
    for rec in records:
        run_cfg = rec.get("config", {})
        stopwatch = rec.get("stopwatch", None)
        group_cfg = _select_group_config(run_cfg)
        key = json.dumps(group_cfg, sort_keys=True)
        if key not in groups:
            groups[key] = {"config": group_cfg, "raw_data": []}
            order.append(key)
        groups[key]["raw_data"].append(stopwatch)
    # Compute aggregates
    result: List[dict] = []
    for key in order:
        item = groups[key]
        raw = item["raw_data"]
        aggregated = _aggregate_profiles([sw for sw in raw if sw is not None])
        result.append({"config": item["config"], "raw_data": raw, "aggregated_data": aggregated})
    return result


def _ping_node(node: str) -> Optional[float]:
    """
    Ping a node and return the average RTT in milliseconds, or None if ping fails.
    """
    try:
        # Use ping with quiet mode, 3 packets, and extract avg time
        result = subprocess.run(
            ["ping", "-q", "-c", "3", node],
            capture_output=True,
            text=True,
            timeout=10
        )
        if result.returncode != 0:
            return None
        # Extract avg time from output like "rtt min/avg/max/mdev = 0.177/0.235/0.270/0.041 ms"
        match = re.search(r"rtt min/avg/max/mdev = [\d.]+/([\d.]+)/[\d.]+/[\d.]+ ms", result.stdout)
        if match:
            return float(match.group(1))
    except (subprocess.TimeoutExpired, ValueError, AttributeError):
        pass
    return None


def _check_ping_latencies(nodes: List[str], label: str) -> None:
    """
    Ping all nodes and print their latencies with a label.
    """
    print(f"\n{label}:", flush=True)
    for node in nodes:
        latency = _ping_node(node)
        if latency is not None:
            print(f"  {node}: {latency:.3f} ms", flush=True)
        else:
            print(f"  {node}: ping failed", flush=True)
    print(flush=True)


def _manage_wan_simulation(cfg: ExperimentConfig, state: str) -> None:
    """
    Enable or disable WAN simulation across cluster nodes.
    state: 'on' or 'off'
    """
    if cfg.exp_setting != "wan":
        return
    
    if cfg.num_parties <= 1:
        # No remote nodes to configure
        return
    
    # Determine script path relative to run_experiment.py location
    script_dir = Path(__file__).parent
    cluster_wan_sim_script = script_dir / "comm" / "cluster-wan-sim.sh"
    
    if not cluster_wan_sim_script.exists():
        print(f"Warning: WAN simulation script not found at {cluster_wan_sim_script}", file=sys.stderr)
        return
    
    # Build list of nodes: node1, node2, ..., node{num_parties-1}
    # (node0 is the current node, so we start from 1)
    nodes = [f"{cfg.node_prefix}{i}" for i in range(1, cfg.num_parties)]
    
    if not nodes:
        return
    
    if state == "on":
        # Check baseline latencies before enabling WAN sim
        _check_ping_latencies(nodes, "Baseline ping latencies (before WAN simulation)")
    
    # Run cluster-wan-sim.sh
    cmd = [str(cluster_wan_sim_script), state] + nodes
    print(f"Managing WAN simulation: {'Enabling' if state == 'on' else 'Disabling'} on nodes {', '.join(nodes)}", flush=True)
    res = subprocess.run(cmd, cwd=str(cluster_wan_sim_script.parent))
    if res.returncode != 0:
        print(f"Warning: cluster-wan-sim.sh returned non-zero exit code {res.returncode}", file=sys.stderr)
        return
    
    if state == "on":
        # Check latencies after enabling WAN sim to verify it's working
        print("Waiting 0.5 seconds for network changes to take effect...", flush=True)
        time.sleep(0.5)
        _check_ping_latencies(nodes, "Ping latencies after enabling WAN simulation")
    elif state == "off":
        # Check latencies after disabling to verify we're back to normal
        print("Waiting 2 seconds for network changes to take effect...", flush=True)
        time.sleep(2)
        _check_ping_latencies(nodes, "Ping latencies after disabling WAN simulation")
        print("Expected: latencies should be back to baseline (< 1ms typically)", flush=True)


def main(argv: Optional[List[str]] = None) -> int:
    arg_list = list(sys.argv[1:] if argv is None else argv)

    parser = build_parser()
    args = parser.parse_args(arg_list)
    cfg = derive_config(args)
    
    # Enable WAN simulation if needed
    wan_sim_enabled = False
    if cfg.exp_setting == "wan":
        _manage_wan_simulation(cfg, "on")
        wan_sim_enabled = True
    
    try:
        # Build and prepare experiment environment
        perform_experiment_setup(cfg)
        # Run experiments
        run_experiments(cfg)
        return 0
    finally:
        # Always disable WAN simulation if we enabled it
        if wan_sim_enabled:
            _manage_wan_simulation(cfg, "off")


if __name__ == "__main__":
    raise SystemExit(main())