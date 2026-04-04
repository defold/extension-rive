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


def load_inline_svg(name: str) -> str:
    svg_path = Path(__file__).resolve().parent / "icons" / name
    return svg_path.read_text(encoding="utf8").strip()


def inline_svg_html(name: str, prefix: str = "") -> str:
    svg = load_inline_svg(name)
    if prefix:
        svg = re.sub(r'id="([^"]+)"', lambda match: f'id="{prefix}{match.group(1)}"', svg)
        svg = re.sub(r'url\(#([^)]+)\)', lambda match: f'url(#{prefix}{match.group(1)})', svg)
        svg = re.sub(r'href="#([^"]+)"', lambda match: f'href="#{prefix}{match.group(1)}"', svg)
        svg = re.sub(r'xlink:href="#([^"]+)"', lambda match: f'xlink:href="#{prefix}{match.group(1)}"', svg)

    return svg.replace(
        "<svg ",
        '<svg class="inline-icon" width="16" height="16" preserveAspectRatio="xMidYMid meet" ',
        1,
    )


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
    platform_name: str,
    run_json: str,
    result_json: str,
    result_data: dict[str, object],
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
    .title-row {{
      display: inline-flex;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
    }}
    .title-row h1 {{
      margin: 0;
    }}
    .platform-row {{
      display: inline-flex;
      align-items: center;
      gap: 8px;
      margin: 0;
      padding: 6px 10px;
      border-radius: 8px;
      background: #223040;
      border: 1px solid #39506b;
      color: #c7d7e6;
      font-size: 12px;
      font-weight: 600;
      flex: 0 0 auto;
    }}
    .inline-icon {{
      width: 16px;
      height: 16px;
      display: block;
    }}
    .platform-text {{
      line-height: 1.2;
    }}
    .likeness-line {{
      display: flex;
      align-items: baseline;
      gap: 8px;
    }}
    .likeness-status.pass {{
      color: #7ee787;
    }}
    .likeness-status.fail {{
      color: #ff7b72;
    }}
    .result-detail {{
      margin: 3px 0;
      color: #b9c7d5;
    }}
    .result-details-body {{
      padding: 10px 12px;
    }}
    .result-details-body .meta {{
      margin-bottom: 8px;
    }}
    .meta {{
      white-space: pre-wrap;
      background: #182028;
      border-radius: 8px;
      padding: 10px 12px;
      line-height: 1.3;
    }}
    details {{
      margin-top: 24px;
      margin-bottom: 24px;
      background: #182028;
      border-radius: 8px;
      padding: 0;
      overflow: hidden;
    }}
    summary, .summary {{
      cursor: pointer;
      padding: 10px 14px;
      font-weight: 600;
      user-select: none;
      font-size: small;
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
    .result-summary {{
      font-size: small;
      margin-top: 16px;
      padding: 0;
      overflow: hidden;
    }}
    .result-summary[open] .result-summary-summary {{
      border-bottom: 1px solid #2b3947;
    }}
    .result-summary-summary {{
      list-style: none;
      display: flex;
      align-items: center;
      gap: 8px;
      cursor: pointer;
      padding: 10px 12px;
      user-select: none;
      font-size: small;
      font-weight: 600;
    }}
    .result-summary-summary::-webkit-details-marker {{
      display: none;
    }}
    .summary-arrow {{
      display: inline-block;
      flex: 0 0 auto;
      width: 0;
      height: 0;
      border-top: 5px solid transparent;
      border-bottom: 5px solid transparent;
      border-left: 7px solid #c7d7e6;
      vertical-align: middle;
      transform: translateY(-1px);
      transition: transform 120ms ease;
    }}
    .likeness-line {{
      display: inline-flex;
      align-items: baseline;
      gap: 8px;
      min-width: 0;
    }}
    .result-summary[open] .summary-arrow {{
      transform: translateY(-1px) rotate(90deg);
    }}
  </style>
</head>
<body>
  <div class="title-row">
    <h1>{escape_html(title)}</h1>
    {f'<div class="platform-row">{platform_badge}</div>' if platform_badge else ''}
  </div>
  <details>
    <summary>Test Info</summary>
    <div class="meta">{escape_html(run_json)}</div>
  </details>
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
