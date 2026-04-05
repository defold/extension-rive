#!/usr/bin/env python3

from __future__ import annotations

import argparse
import html
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path


def escape_html(value: object) -> str:
    return html.escape(str(value), quote=True)


def copy_report_css(destination_dir: Path) -> None:
    stylesheet_path = Path(__file__).resolve().parent / "report.css"
    shutil.copyfile(stylesheet_path, destination_dir / "report.css")


def load_inline_svg(name: str) -> str:
    svg_path = Path(__file__).resolve().parent / "icons" / name
    return svg_path.read_text(encoding="utf8").strip()


def inline_svg_html(name: str, prefix: str = "") -> str:
    _ = prefix
    svg = load_inline_svg(name)
    return f'<span class="inline-icon">{svg}</span>'


def resolve_compare_command() -> tuple[list[str], str]:
    compare = shutil.which("compare")
    if compare:
        return [compare], "compare"

    magick = shutil.which("magick")
    if magick:
        return [magick, "compare"], "magick compare"

    raise RuntimeError("Could not find ImageMagick compare executable. Expected either 'compare' or 'magick' on PATH.")


def compare_images(result_path: Path, expected_path: Path, report_dir: Path) -> dict[str, object]:
    diff_image_name = "diff.png"
    diff_image_path = report_dir / diff_image_name
    command_prefix, display_name = resolve_compare_command()
    args = [*command_prefix, "-metric", "RMSE", str(result_path), str(expected_path), str(diff_image_path)]
    print(f"Running: {display_name} {' '.join(args[len(command_prefix):])}")
    completed = subprocess.run(args, capture_output=True, text=True)
    raw_output = f"{completed.stdout or ''}{completed.stderr or ''}".strip()

    if completed.returncode not in (0, 1):
        raise RuntimeError(raw_output or f"ImageMagick compare exited with status {completed.returncode}")

    match = re.search(r"\(([0-9.+\-eE]+)\)", raw_output)
    if not match:
        raise RuntimeError(f"Could not parse ImageMagick compare output: {raw_output}")

    normalized_difference = float(match.group(1))
    likeness_percent = max(0.0, min(100.0, (1.0 - normalized_difference) * 100.0))

    return {
        "metric": "RMSE",
        "raw_output": raw_output,
        "normalized_difference": normalized_difference,
        "likeness_percent": likeness_percent,
        "diff_image_name": diff_image_name,
    }


CRASH_SIGNATURES = (
    re.compile(r"\bFATAL EXCEPTION\b"),
    re.compile(r"\bFatal signal \d+\b"),
    re.compile(r"\bSIG(?:SEGV|ABRT|BUS|ILL)\b"),
    re.compile(r"\bAndroidRuntime\b"),
    re.compile(r"\b(?:INFO|ERROR):CRASH\b"),
    re.compile(r"\bProcess .* has died\b"),
)


def resolve_console_log_path(run_data: dict[str, object], report_dir: Path) -> Path | None:
    for key in ("console_log_path", "logcat_path", "console_log_raw_path", "logcat_raw_path"):
        raw_path = run_data.get(key)
        if not isinstance(raw_path, str) or not raw_path:
            continue

        candidate = Path(raw_path)
        if not candidate.is_absolute():
            candidate = candidate.resolve()
        if candidate.is_file():
            return candidate

    for fallback_name in ("console.log", "logcat.txt", "logcat.raw.txt"):
        candidate = report_dir / fallback_name
        if candidate.is_file():
            return candidate

    return None


def resolve_console_log_href(run_data: dict[str, object], report_dir: Path) -> str | None:
    console_log_path = resolve_console_log_path(run_data, report_dir)
    if console_log_path is None:
        return None

    try:
        return console_log_path.relative_to(report_dir).as_posix()
    except ValueError:
        try:
            return console_log_path.relative_to(report_dir.parent).as_posix()
        except ValueError:
            return console_log_path.as_posix()


def detect_android_crash(run_data: dict[str, object], report_dir: Path) -> dict[str, object] | None:
    if str(run_data.get("mode", "")).strip().lower() != "android":
        return None

    console_log_path = resolve_console_log_path(run_data, report_dir)
    if console_log_path is None:
        return None

    log_text = console_log_path.read_text(encoding="utf8", errors="replace")
    log_lines = log_text.splitlines()
    if not log_lines:
        return None

    crash_line_indexes = [index for index, line in enumerate(log_lines) if any(signature.search(line) for signature in CRASH_SIGNATURES)]
    if not crash_line_indexes:
        return None

    first_index = max(0, crash_line_indexes[0] - 5)
    last_index = min(len(log_lines), first_index + 250)
    snippet_lines = log_lines[first_index:last_index]
    if last_index < len(log_lines):
        snippet_lines.append("... truncated ...")

    signature_line = log_lines[crash_line_indexes[0]].strip()
    details = "\n".join(snippet_lines).strip()
    if not details:
        return None

    return {
        "detected": True,
        "signature": signature_line,
        "details": details,
        "log_path": str(console_log_path),
    }


def build_result_data(run_data: dict[str, object], likeness_threshold: float, expected_screenshot: Path | None, report_dir: Path) -> dict[str, object]:
    result_data: dict[str, object] = {}
    if expected_screenshot is None:
        result_data["status"] = "fail"
        return result_data

    expected_name = "expected.png"
    shutil.copyfile(expected_screenshot, report_dir / expected_name)

    comparison = compare_images(Path(str(run_data["screenshot_path"])), report_dir / expected_name, report_dir)
    result_data.update(comparison)
    result_data["status"] = "pass" if float(result_data["likeness_percent"]) >= likeness_threshold else "fail"
    return result_data


def build_html(
    title: str,
    platform_name: str,
    run_json: str,
    result_json: str,
    result_data: dict[str, object],
    crash_data: dict[str, object] | None,
    console_log_href: str | None,
    expected_screenshot: Path | None,
    captured_screenshot: Path,
) -> str:
    platform_key = platform_name.lower().strip()
    if platform_key.endswith("android"):
        platform_icon_name = "android.svg"
    elif platform_key.endswith("-web"):
        platform_icon_name = "html5.svg"
    else:
        platform_icon_name = ""

    platform_icon = inline_svg_html(platform_icon_name, prefix="platform-") if platform_icon_name else ""

    if "likeness_percent" in result_data:
        status_pass = result_data.get("status") == "pass"
        result_header = (
            f'<summary class="result-summary-summary">'
            f'<span class="summary-arrow" aria-hidden="true"></span>'
            f'<span class="likeness-line">'
            f'<span class="likeness-value">{float(result_data["likeness_percent"]):.2f}%</span> '
            f'<span class="likeness-label">Likeness</span> '
            f'<span class="likeness-status {"pass" if status_pass else "fail"}">'
            f'{"✅ PASS" if status_pass else "❌ FAIL"}'
            f'</span>'
            f'</span>'
            f'</summary>'
        )
        result_body = f"""
      <div class="result-details-body">
        <p class="result-detail">Threshold: {escape_html(f"{float(result_data.get('likeness_threshold', 0.0)):.2f}")}%</p>
        <p class="result-detail">Metric: {escape_html(result_data.get('metric', 'n/a'))}</p>
        <p class="result-detail">Normalized difference: {escape_html(f"{float(result_data.get('normalized_difference', 0.0)):.6f}")}</p>
        <div class="meta">{escape_html(result_json)}</div>
        <img src="{escape_html(result_data.get('diff_image_name', ''))}" alt="Difference image from ImageMagick compare">
      </div>
    """
    else:
        result_header = '<summary class="result-summary-summary"><span class="summary-arrow" aria-hidden="true"></span><span class="likeness-line"><span class="likeness-label">No comparison result available</span></span></summary>'
        result_body = '<div class="result-details-body"><p class="result-detail">No comparison result available.</p></div>'

    crash_body = ""
    if crash_data:
        crash_body = f"""
    <details class="crash-summary">
      <summary class="crash-summary-summary">
        <span class="summary-arrow" aria-hidden="true"></span>
        <span class="crash-line">
          <span class="crash-mark" aria-hidden="true">❌</span>
          <span class="crash-label">Crash</span>
        </span>
      </summary>
      <div class="result-details-body">
        <p class="result-detail">Detected in console.log</p>
        <p class="result-detail">Signature: {escape_html(crash_data.get("signature", "n/a"))}</p>
        <div class="meta">{escape_html(crash_data.get("details", ""))}</div>
      </div>
    </details>
    """

    expected_body = (
        '<img src="expected.png" alt="Expected render screenshot">'
        if expected_screenshot is not None
        else "<p>No expected screenshot provided.</p>"
    )

    platform_badge = (
        f'{platform_icon}<span class="platform-text">Platform: <strong>{escape_html(platform_name)}</strong></span>'
        if platform_name
        else ""
    )

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Render Test Report</title>
  <link rel="stylesheet" href="report.css">
</head>
<body>
  <div class="title-row">
    <h1>{escape_html(title)}</h1>
    {f'<div class="platform-row">{platform_badge}</div>' if platform_badge else ''}
  </div>
  <details>
    <summary>Test Info</summary>
    <div class="result-details-body">
      {"<p class=\"result-detail\">Console log: <a href=\"" + escape_html(console_log_href) + "\">console.log</a></p>" if console_log_href else ""}
      <div class="meta">{escape_html(run_json)}</div>
    </div>
  </details>
  {crash_body}
  <details class="panel result-summary">
    {result_header}
    {result_body}
  </details>
  <div class="grid">
    <section class="panel">
      <h2>Captured:</h2>
      <img src="{escape_html(captured_screenshot.name)}" alt="Render test result screenshot">
    </section>
    <section class="panel">
      <h2>Expected:</h2>
      {expected_body}
    </section>
  </div>
</body>
</html>
"""


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build a render-test report from captured artifacts.")
    parser.add_argument("--run-json", required=True, type=Path)
    parser.add_argument("--result-json", required=True, type=Path)
    parser.add_argument("--index", required=True, type=Path)
    parser.add_argument("--expected-screenshot", type=Path)
    parser.add_argument("--likeness", type=float, default=95.0)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    run_data = json.loads(args.run_json.read_text(encoding="utf8"))

    report_dir = args.index.parent
    report_dir.mkdir(parents=True, exist_ok=True)
    copy_report_css(report_dir)

    expected_screenshot = args.expected_screenshot
    if expected_screenshot is not None and not expected_screenshot.is_absolute():
        expected_screenshot = expected_screenshot.resolve()
    if expected_screenshot is not None and not expected_screenshot.exists():
        raise FileNotFoundError(f"expected screenshot not found: {expected_screenshot}")

    if expected_screenshot is None:
        raw_expected = run_data.get("expected_screenshot_path")
        if isinstance(raw_expected, str) and raw_expected:
            expected_screenshot = Path(raw_expected)
            if not expected_screenshot.is_absolute():
                expected_screenshot = expected_screenshot.resolve()
            if not expected_screenshot.exists():
                raise FileNotFoundError(f"expected screenshot not found: {expected_screenshot}")

    result_data = build_result_data(run_data, args.likeness, expected_screenshot, report_dir)
    result_data["likeness_threshold"] = args.likeness
    console_log_href = resolve_console_log_href(run_data, report_dir)
    crash_data = detect_android_crash(run_data, report_dir)
    if crash_data is not None:
        result_data["crash_detected"] = True
        result_data["crash_signature"] = crash_data["signature"]
        result_data["crash_log_path"] = crash_data["log_path"]
        result_data["status"] = "fail"

    result_json = json.dumps(result_data, indent=2)
    args.result_json.write_text(f"{result_json}\n", encoding="utf8")

    run_json = json.dumps(run_data, indent=2)
    title = str(run_data.get("test_name") or run_data.get("collection") or "Render Test")
    platform_name = str(run_data.get("platform") or report_dir.parent.parent.name)
    captured_screenshot = Path(str(run_data["screenshot_path"]))
    if not captured_screenshot.is_absolute():
        captured_screenshot = captured_screenshot.resolve()

    html_text = build_html(
        title=title,
        platform_name=platform_name,
        run_json=run_json,
        result_json=result_json,
        result_data=result_data,
        crash_data=crash_data,
        console_log_href=console_log_href,
        expected_screenshot=expected_screenshot,
        captured_screenshot=captured_screenshot,
    )
    args.index.write_text(html_text, encoding="utf8")

    if result_data.get("status") != "pass":
        return 2 if ("likeness_percent" in result_data or crash_data is not None) else 0

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except Exception as exc:  # noqa: BLE001
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
