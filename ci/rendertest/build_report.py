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
    run_json: str,
    result_json: str,
    result_data: dict[str, object],
    expected_screenshot: Path | None,
    captured_screenshot: Path,
) -> str:
    if "likeness_percent" in result_data:
        result_header = (
            f"{float(result_data['likeness_percent']):.2f}% Likeness - "
            f"{'✅ PASS' if result_data.get('status') == 'pass' else '❌ FAIL'}"
        )
        result_body = f"""
        <details class="result-details">
      <summary>Detailed Info</summary>
      <div class="result-details-body">
        <p class="result-detail">Threshold: {escape_html(f"{float(result_data.get('likeness_threshold', 0.0)):.2f}")}%</p>
        <p class="result-detail">Metric: {escape_html(result_data.get('metric', 'n/a'))}</p>
        <p class="result-detail">Normalized difference: {escape_html(f"{float(result_data.get('normalized_difference', 0.0)):.6f}")}</p>
        <div class="meta">{escape_html(result_json)}</div>
        <img src="{escape_html(result_data.get('diff_image_name', ''))}" alt="Difference image from ImageMagick compare">
      </div>
    </details>"""
    else:
        result_header = "No comparison result available"
        result_body = '<p class="result-detail">No comparison result available.</p>'

    expected_body = (
        '<img src="expected.png" alt="Expected render screenshot">'
        if expected_screenshot is not None
        else "<p>No expected screenshot provided.</p>"
    )

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>Render Test Report</title>
  <style>
    body {{
      margin: 24px;
      font-family: Menlo, Monaco, Consolas, monospace;
      background: #101418;
      color: #d7e0ea;
    }}
    h1, h2 {{
      font-weight: 600;
    }}
    .grid {{
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 24px;
      margin-top: 24px;
    }}
    .panel {{
      background: #182028;
      border-radius: 8px;
      padding: 16px;
    }}
    .result-summary {{
      margin-top: 24px;
    }}
    .result-detail {{
      margin: 6px 0;
      color: #b9c7d5;
    }}
    .result-details {{
      margin-top: 16px;
      background: #101418;
      border-radius: 8px;
      overflow: hidden;
    }}
    .result-details summary {{
      padding: 12px 14px;
      font-weight: 600;
    }}
    .result-details[open] summary {{
      border-bottom: 1px solid #2b3947;
    }}
    .result-details-body {{
      padding: 16px;
    }}
    .result-details-body .meta {{
      margin-bottom: 16px;
    }}
    .meta {{
      white-space: pre-wrap;
      background: #182028;
      border-radius: 8px;
      padding: 16px;
      line-height: 1.5;
    }}
    details {{
      margin-top: 24px;
      margin-bottom: 24px;
      background: #182028;
      border-radius: 8px;
      padding: 0;
      overflow: hidden;
    }}
    summary {{
      cursor: pointer;
      padding: 14px 16px;
      font-weight: 600;
      user-select: none;
    }}
    details[open] summary {{
      border-bottom: 1px solid #2b3947;
    }}
    details .meta {{
      margin: 0;
      border-radius: 0;
      background: transparent;
    }}
    img {{
      display: block;
      max-width: 100%;
      height: auto;
      border-radius: 8px;
      border: 1px solid #2b3947;
      background: #0b0f13;
    }}
  </style>
</head>
<body>
  <h1>{escape_html(title)}</h1>
  <details>
    <summary>Meta Info</summary>
    <div class="meta">{escape_html(run_json)}</div>
  </details>
  <section class="panel result-summary">
    <h2>{escape_html(result_header)}</h2>
    {result_body}
  </section>
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

    result_json = json.dumps(result_data, indent=2)
    args.result_json.write_text(f"{result_json}\n", encoding="utf8")

    run_json = json.dumps(run_data, indent=2)
    title = str(run_data.get("test_name") or run_data.get("collection") or "Render Test")
    captured_screenshot = Path(str(run_data["screenshot_path"]))
    if not captured_screenshot.is_absolute():
        captured_screenshot = captured_screenshot.resolve()

    html_text = build_html(
        title=title,
        run_json=run_json,
        result_json=result_json,
        result_data=result_data,
        expected_screenshot=expected_screenshot,
        captured_screenshot=captured_screenshot,
    )
    args.index.write_text(html_text, encoding="utf8")

    if result_data.get("status") != "pass":
        return 2 if "likeness_percent" in result_data else 0

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except Exception as exc:  # noqa: BLE001
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
