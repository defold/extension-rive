#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
from datetime import datetime, timezone
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build a human-readable Android system trace report.")
    parser.add_argument("--run-json", required=True, type=Path, help="Path to the run.json for this render test.")
    parser.add_argument("--output", required=True, type=Path, help="Output system-trace.json path.")
    return parser.parse_args()


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf8"))


def read_text(path: str | Path) -> str:
    candidate = Path(path)
    if not candidate.is_file():
        return ""
    return candidate.read_text(encoding="utf8", errors="replace")


def clean_lines(text: str) -> list[str]:
    return [line.rstrip() for line in text.splitlines()]


ANSI_ESCAPE_RE = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]")


def strip_ansi(text: str) -> str:
    return ANSI_ESCAPE_RE.sub("", text)


def first_matching_line(lines: list[str], pattern: str) -> str:
    regex = re.compile(pattern)
    for line in lines:
        if regex.search(line):
            return line.strip()
    return ""


def collect_matching_lines(lines: list[str], pattern: str) -> list[str]:
    regex = re.compile(pattern)
    return [line.strip() for line in lines if regex.search(line)]


def format_decimal(value: float) -> str:
    if value.is_integer():
        return str(int(value))
    return f"{value:.1f}".rstrip("0").rstrip(".")


def kib_to_mib(kib: int) -> float:
    return kib / 1024.0


def parse_top_cpu_percent(line: str) -> float | None:
    tokens = strip_ansi(line).split()
    if len(tokens) < 9:
        return None
    percent_token = tokens[8]
    if not percent_token.endswith("%"):
        try:
            return float(percent_token)
        except ValueError:
            return None
    try:
        return float(percent_token[:-1])
    except ValueError:
        return None


def parse_top_process_name(line: str) -> str:
    cleaned = strip_ansi(line).strip()
    tokens = cleaned.split()
    if len(tokens) < 11:
        return ""
    return " ".join(tokens[11:]).rstrip("+").strip()


def is_top_process_line(line: str) -> bool:
    tokens = strip_ansi(line).split()
    return len(tokens) >= 11 and tokens[0].isdigit()


def parse_topinfo(text: str, package_name: str, pid: str) -> dict[str, object]:
    lines = clean_lines(text)
    process_lines = [line.strip() for line in lines if is_top_process_line(line)]

    app_lines = []
    if package_name:
        app_lines.extend(line for line in process_lines if package_name in strip_ansi(line))
    if pid:
        app_lines.extend(line for line in process_lines if re.search(rf"\b{re.escape(pid)}\b", strip_ansi(line)))

    seen: set[str] = set()
    app_candidates: list[dict[str, object]] = []
    for line in app_lines:
        if line in seen:
            continue
        seen.add(line)
        percent = parse_top_cpu_percent(line)
        if percent is None:
            continue
        name = parse_top_process_name(line)
        app_candidates.append(
            {
                "line": line.strip(),
                "name": name,
                "percent": percent,
            }
        )

    selected_app = None
    if app_candidates:
        selected_app = max(app_candidates, key=lambda candidate: float(candidate["percent"]))

    if selected_app:
        summary = f"CPU {format_decimal(float(selected_app['percent']))} %"
    else:
        summary = "CPU usage unavailable."

    return {
        "summary": summary,
        "backend": "top",
        "selected_app": selected_app,
    }


def parse_meminfo(text: str) -> dict[str, object]:
    lines = clean_lines(text)
    app_summary_start = None
    for index, line in enumerate(lines):
        if line.strip() == "App Summary":
            app_summary_start = index + 1
            break

    app_summary_lines: list[str] = []
    if app_summary_start is not None:
        for line in lines[app_summary_start:]:
            stripped = line.strip()
            if stripped == "Objects" or stripped == "SQL" or stripped == "Asset Allocations":
                break
            if not stripped:
                continue
            app_summary_lines.append(line)

    heap_entries: list[dict[str, object]] = []
    heap_patterns = (
        r"^\s*(Java Heap|Native Heap|Dalvik Heap):\s+(\d+)\s*$",
    )
    for line in app_summary_lines:
        for pattern in heap_patterns:
            match = re.match(pattern, line)
            if not match:
                continue
            label = match.group(1)
            try:
                kib = int(match.group(2))
            except ValueError:
                continue
            heap_entries.append(
                {
                    "label": label,
                    "kib": kib,
                    "mib": kib_to_mib(kib),
                }
            )
            break

    if not heap_entries:
        for line in lines:
            match = re.match(r"^\s*(Java Heap|Native Heap|Dalvik Heap)\s+(\d+)\s+", line)
            if not match:
                continue
            try:
                kib = int(match.group(2))
            except ValueError:
                continue
            heap_entries.append(
                {
                    "label": match.group(1),
                    "kib": kib,
                    "mib": kib_to_mib(kib),
                }
            )

    total_pss_kib = None
    total_swap_pss_kib = None
    for line in app_summary_lines or lines:
        match = re.match(r"^\s*TOTAL:\s+(\d+)\s+.*TOTAL SWAP PSS:\s+(\d+)", line)
        if match:
            total_pss_kib = int(match.group(1))
            total_swap_pss_kib = int(match.group(2))
            break

    selected_heap = max(heap_entries, key=lambda entry: int(entry["kib"])) if heap_entries else None
    if total_pss_kib is not None:
        summary = f"Memory: {format_decimal(kib_to_mib(total_pss_kib))} mb (app)"
    elif selected_heap:
        summary = f"Memory: {format_decimal(float(selected_heap['mib']))} mb (heap)"
    else:
        summary = "Memory usage unavailable."

    return {
        "summary": summary,
        "backend": "meminfo",
        "selected_heap": selected_heap,
        "heap_entries": heap_entries,
        "app_summary_lines": app_summary_lines,
        "total_pss_kib": total_pss_kib,
        "total_swap_pss_kib": total_swap_pss_kib,
    }


def parse_display(text: str) -> dict[str, object]:
    lines = clean_lines(text)
    metrics: dict[str, str] = {}
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("Physical size:"):
            metrics["physical_size"] = stripped.split(":", 1)[1].strip()
        elif stripped.startswith("Override size:"):
            metrics["override_size"] = stripped.split(":", 1)[1].strip()
        elif stripped.startswith("Physical density:"):
            metrics["physical_density"] = stripped.split(":", 1)[1].strip()
        elif stripped.startswith("Override density:"):
            metrics["override_density"] = stripped.split(":", 1)[1].strip()

    refresh_rate = first_matching_line(lines, r"refreshRate[=:]\s*([0-9.]+)")
    if refresh_rate:
        metrics["refresh_rate"] = refresh_rate

    summary_parts = []
    if metrics.get("physical_size"):
        summary_parts.append(f"{metrics['physical_size']}")
    if metrics.get("physical_density"):
        summary_parts.append(f"{metrics['physical_density']} dpi")
    if metrics.get("refresh_rate"):
        summary_parts.append(f"{metrics['refresh_rate']} Hz")

    summary = " @ ".join(summary_parts[:2]) if summary_parts else "Display metrics unavailable."
    if len(summary_parts) > 2:
        summary = f"{summary}, {summary_parts[2]}"

    return {
        "summary": summary,
        "backend": "display",
        "metrics": metrics,
    }


def parse_trace(trace_path: Path, backend: str) -> dict[str, object]:
    if not trace_path.is_file():
        return {
            "backend": backend,
            "path": str(trace_path),
            "exists": False,
            "summary": "Trace file not found.",
        }

    size_bytes = trace_path.stat().st_size
    if backend == "atrace":
        text = trace_path.read_text(encoding="utf8", errors="replace")
        lines = clean_lines(text)
        begin_events = sum(1 for line in lines if line.startswith("B|"))
        end_events = sum(1 for line in lines if line.startswith("E|"))
        counter_events = sum(1 for line in lines if line.startswith("C|"))
        return {
            "backend": backend,
            "path": str(trace_path),
            "exists": True,
            "size_bytes": size_bytes,
            "summary": f"text trace with {len(lines)} lines",
            "line_count": len(lines),
            "begin_events": begin_events,
            "end_events": end_events,
            "counter_events": counter_events,
        }

    return {
        "backend": backend,
        "path": str(trace_path),
        "exists": True,
        "size_bytes": size_bytes,
        "summary": "binary perfetto trace captured",
        "note": "perfetto trace was captured as a binary file; this report summarizes device snapshots and trace metadata.",
    }


def compose_summary(cpu_usage: dict[str, object], memory_usage: dict[str, object], trace_counters: dict[str, object], display_metrics: dict[str, object]) -> str:
    parts = []
    if cpu_usage and cpu_usage.get("summary"):
        parts.append(str(cpu_usage["summary"]))
    if memory_usage and memory_usage.get("summary"):
        parts.append(str(memory_usage["summary"]))
    if display_metrics and display_metrics.get("summary"):
        parts.append(f"Display: {display_metrics['summary']}")
    return " | ".join(parts) if parts else "No Android system trace data was collected."


def build_report(run_data: dict[str, object]) -> dict[str, object]:
    system_trace_enabled = bool(run_data.get("system_trace"))
    backend = str(run_data.get("system_trace_backend") or "").strip()
    trace_path = Path(str(run_data.get("system_trace_path") or ""))
    top_path = Path(str(run_data.get("system_top_path") or ""))
    meminfo_path = Path(str(run_data.get("system_meminfo_path") or ""))
    display_path = Path(str(run_data.get("system_display_path") or ""))

    top_text = read_text(top_path)
    meminfo_text = read_text(meminfo_path)
    display_text = read_text(display_path)

    cpu_usage = parse_topinfo(top_text, str(run_data.get("package_name") or ""), str(run_data.get("app_pid") or ""))
    memory_usage = parse_meminfo(meminfo_text)
    display_metrics = parse_display(display_text)
    trace_counters = parse_trace(trace_path, backend)

    summary = compose_summary(cpu_usage, memory_usage, trace_counters, display_metrics)

    return {
        "generated_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "system_trace_enabled": system_trace_enabled,
        "backend": backend,
        "package_name": run_data.get("package_name", ""),
        "app_pid": run_data.get("app_pid", ""),
        "Summary": summary,
        "Trace counters": trace_counters,
        "CPU usage": {
            "summary": cpu_usage["summary"],
            "backend": cpu_usage["backend"],
            "selected_app": cpu_usage["selected_app"],
            "source_path": str(top_path),
        },
        "Memory usage": {
            "summary": memory_usage["summary"],
            "backend": memory_usage["backend"],
            "selected_heap": memory_usage["selected_heap"],
            "heap_entries": memory_usage["heap_entries"],
            "total_pss_kib": memory_usage["total_pss_kib"],
            "total_swap_pss_kib": memory_usage["total_swap_pss_kib"],
            "source_path": str(meminfo_path),
        },
        "Display metrics": {
            "summary": display_metrics["summary"],
            "backend": display_metrics["backend"],
            "metrics": display_metrics["metrics"],
            "source_path": str(display_path),
        },
    }


def main() -> int:
    args = parse_args()
    run_data = load_json(args.run_json)
    report = build_report(run_data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
    print(f"Wrote Android system trace report: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
