#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from html import escape
from pathlib import Path



PASS_ICON = "✅"
FAIL_ICON = "❌"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Assemble individual render test reports into one HTML summary.",
    )
    parser.add_argument(
        "--title",
        default="Render Test Summary",
        help='Page title heading. Default: "Render Test Summary"',
    )
    parser.add_argument(
        "--subtitle",
        default="",
        help="Optional subtitle shown below the title.",
    )
    parser.add_argument(
        "--reports-root",
        default="build/render-tests",
        help="Root directory containing per-test report folders. Default: build/render-tests",
    )
    parser.add_argument(
        "--output",
        default="build/render-tests/index.html",
        help="Output HTML summary path. Default: build/render-tests/index.html",
    )
    parser.add_argument(
        "--summary-json",
        default="build/render-tests/summary.json",
        help="JSON file to write aggregate summary data to. Default: build/render-tests/summary.json",
    )
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf8") as fh:
        return json.load(fh)


def load_inline_svg(name: str) -> str:
    svg_path = Path(__file__).resolve().parent / "icons" / name
    return svg_path.read_text(encoding="utf8").strip()


def collect_groups(reports_root: Path, output_path: Path) -> list[dict]:
    groups: dict[str, dict] = {}

    for run_json_path in sorted(reports_root.glob("**/report/run.json")):
        report_dir = run_json_path.parent
        result_json_path = report_dir / "result.json"
        index_path = report_dir / "index.html"

        if not result_json_path.is_file() or not index_path.is_file():
            continue

        run_data = load_json(run_json_path)
        result_data = load_json(result_json_path)

        screenshot_name = Path(run_data["screenshot_path"]).name
        screenshot_path = report_dir / screenshot_name
        if not screenshot_path.is_file():
            screenshot_path = None

        test_name = run_data.get("test_name") or run_data.get("collection") or run_json_path.parent.parent.name
        group_name = str(run_data.get("test_group") or report_dir.parent.name or test_name).strip()
        description = str(run_data.get("description") or "").strip()
        likeness = result_data.get("likeness_percent")
        status = result_data.get("status", "fail")
        platform_name = str(run_data.get("platform") or (run_json_path.parents[2].name if len(run_json_path.parents) > 2 else ""))

        group = groups.setdefault(
            group_name,
            {
                "name": group_name,
                "description": "",
                "tests": [],
            },
        )
        if not group["description"] and description:
            group["description"] = description

        group["tests"].append(
            {
                "name": test_name,
                "group_name": group_name,
                "collection": run_data.get("collection", ""),
                "platform": platform_name,
                "is_html5": platform_name.endswith("-web"),
                "status": status,
                "status_icon": PASS_ICON if status == "pass" else FAIL_ICON,
                "likeness": likeness,
                "likeness_text": f"{likeness:.2f}%" if isinstance(likeness, (int, float)) else "n/a",
                "index_href": index_path.relative_to(output_path.parent).as_posix(),
                "thumbnail_src": screenshot_path.relative_to(output_path.parent).as_posix() if screenshot_path else "",
            }
        )

    grouped_tests = []
    for group_name in sorted(groups.keys(), key=lambda value: value.lower()):
        group = groups[group_name]
        group["tests"].sort(key=lambda item: (item["platform"].lower(), item["name"].lower(), item["index_href"].lower()))
        grouped_tests.append(group)

    return grouped_tests


def render_html(title: str, subtitle: str, groups: list[dict]) -> str:
    tests = [test for group in groups for test in group["tests"]]
    pass_count = sum(1 for test in tests if test["status"] == "pass")
    fail_count = sum(1 for test in tests if test["status"] != "pass")
    html5_icon = load_inline_svg("html5.svg").replace(
        "<svg ",
        '<svg class="inline-icon" width="16" height="16" preserveAspectRatio="xMidYMid meet" ',
        1,
    )

    rows = []
    for group in groups:
        thumbs = []
        for test in group["tests"]:
            thumbnail = (
                f'<img src="{escape(test["thumbnail_src"])}" alt="{escape(test["name"])} thumbnail">'
                if test["thumbnail_src"]
                else '<div class="missing-thumb">No screenshot</div>'
            )
            overlay = (
                f'<div class="thumb-badge" title="Html5">{html5_icon}</div>'
                if test["is_html5"]
                else ""
            )
            thumbs.append(
                f"""<a class="thumb-card {escape(test["status"])}" href="{escape(test["index_href"])}">
  <div class="thumb">{thumbnail}{overlay}</div>
  <div class="thumb-caption">
    <span class="thumb-status">{escape(test["status_icon"])} {escape(test["likeness_text"])}</span>
    <span class="thumb-platform">{escape(test["platform"])}</span>
  </div>
</a>"""
            )

        group_description = escape(group["description"]) if group["description"] else "No description available."
        rows.append(
            f"""<article class="group">
  <header class="group-header">
    <h2 class="group-name">{escape(group["name"])}</h2>
    <p class="group-description">{group_description}</p>
  </header>
  <div class="thumb-strip">
    {"".join(thumbs)}
  </div>
</article>"""
        )

    rows_html = "\n".join(rows) if rows else '<p class="empty">No test reports found.</p>'

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>{escape(title)}</title>
  <style>
    body {{
      margin: 24px;
      font-family: Menlo, Monaco, Consolas, monospace;
      background: #101418;
      color: #d7e0ea;
    }}
    h1 {{
      margin-bottom: 8px;
    }}
    .subtitle {{
      margin: 0 0 24px;
      color: #9fb0c0;
      font-size: 15px;
    }}
    .totals {{
      display: flex;
      gap: 16px;
      flex-wrap: wrap;
      margin-bottom: 24px;
    }}
    .totals p {{
      margin: 0;
      padding: 10px 14px;
      background: #182028;
      border-radius: 8px;
      font-size: 18px;
    }}
    .groups {{
      display: flex;
      flex-direction: column;
      gap: 20px;
    }}
    .group {{
      background: #182028;
      border-radius: 10px;
      padding: 16px;
      border: 1px solid #2b3947;
    }}
    .group-header {{
      margin-bottom: 14px;
    }}
    .group-name {{
      margin: 0 0 6px;
      font-size: 18px;
    }}
    .group-description {{
      margin: 0;
      color: #9fb0c0;
      font-size: 13px;
      line-height: 1.4;
    }}
    .thumb-strip {{
      display: flex;
      gap: 16px;
      flex-wrap: wrap;
    }}
    .thumb-card {{
      width: 180px;
      color: inherit;
      text-decoration: none;
      display: block;
    }}
    .thumb {{
      position: relative;
      aspect-ratio: 16 / 9;
      background: #0b0f13;
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
      border-bottom: 1px solid #2b3947;
    }}
    .thumb img {{
      width: 100%;
      height: 100%;
      object-fit: contain;
      display: block;
      position: relative;
      z-index: 0;
    }}
    .missing-thumb {{
      color: #8fa1b2;
      font-size: 14px;
    }}
    .thumb-badge {{
      position: absolute;
      top: 10px;
      right: 10px;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      padding: 3px 5px;
      border-radius: 6px;
      background: rgba(16, 20, 24, 0.7);
      border: 1px solid rgba(201, 213, 228, 0.28);
      backdrop-filter: blur(4px);
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.32);
      z-index: 2;
    }}
    .inline-icon {{
      width: 16px;
      height: 16px;
      display: block;
      min-width: 16px;
      min-height: 16px;
    }}
    .thumb-caption {{
      display: flex;
      flex-direction: column;
      gap: 4px;
      padding-top: 8px;
    }}
    .thumb-status {{
      font-size: 13px;
      font-weight: 600;
    }}
    .thumb-platform {{
      color: #9fb0c0;
      font-size: 12px;
      word-break: break-word;
    }}
  </style>
</head>
<body>
  <h1>{escape(title)}</h1>
  {f'<p class="subtitle">{escape(subtitle)}</p>' if subtitle else ""}
  <div class="totals">
    <p>{PASS_ICON} {pass_count} passed</p>
    <p>{FAIL_ICON} {fail_count} failed</p>
  </div>
  <section class="groups">
    {rows_html}
  </section>
</body>
</html>
"""


def main() -> int:
    args = parse_args()
    reports_root = Path(args.reports_root).resolve()
    output_path = Path(args.output).resolve()

    groups = collect_groups(reports_root, output_path)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(render_html(args.title, args.subtitle, groups), encoding="utf8")

    tests = [test for group in groups for test in group["tests"]]
    pass_count = sum(1 for test in tests if test["status"] == "pass")
    fail_count = sum(1 for test in tests if test["status"] != "pass")
    total_count = len(tests)

    summary_json_path = Path(args.summary_json).resolve()
    summary_json_path.parent.mkdir(parents=True, exist_ok=True)
    summary_json_path.write_text(
        json.dumps(
            {
                "pass_count": pass_count,
                "fail_count": fail_count,
                "total_count": total_count,
            },
            indent=2,
        )
        + "\n",
        encoding="utf8",
    )

    print(f"{PASS_ICON} {pass_count} passed")
    print(f"{FAIL_ICON} {fail_count} failed")
    print(f"Wrote summary: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
