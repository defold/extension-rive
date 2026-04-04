#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
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


def platform_label(platform: str) -> str:
    return platform if platform else "empty platform"


def platform_icon_name(platform: str) -> str:
    platform = platform.lower().strip()
    if platform.endswith("android"):
        return "android.svg"
    if platform.endswith("-web"):
        return "html5.svg"
    return ""


def platform_display_html(platform: str, label: str, prefix: str = "") -> str:
    if platform == "__total__":
        return f'<span class="platform-label"><span>{escape(label)}</span></span>'

    icon_name = platform_icon_name(platform)
    icon_html = ""
    if icon_name:
        icon_html = inline_svg_html(icon_name, prefix=prefix)
    label_html = escape(label)
    if icon_html:
        return f'<span class="platform-label">{icon_html}<span>{label_html}</span></span>'
    return f'<span class="platform-label"><span>{label_html}</span></span>'


def collect_groups(reports_root: Path, output_path: Path) -> list[dict]:
    groups: dict[str, dict] = {}

    for run_json_path in sorted(reports_root.glob("**/run.json")):
        if run_json_path.name != "run.json":
            continue

        report_dir = run_json_path.parent
        run_data = load_json(run_json_path)
        result_data: dict[str, object] = {}
        index_path: Path | None = None
        screenshot_path: Path | None = None

        if report_dir.name == "report":
            result_json_path = report_dir / "result.json"
            index_path = report_dir / "index.html"
            if not result_json_path.is_file() or not index_path.is_file():
                continue
            result_data = load_json(result_json_path)
            screenshot_name = Path(run_data["screenshot_path"]).name
            screenshot_path = report_dir / screenshot_name
        else:
            screenshot_name = Path(run_data["screenshot_path"]).name
            screenshot_path = report_dir / screenshot_name
            index_path = screenshot_path if screenshot_path.is_file() else run_json_path

        if screenshot_path is not None and not screenshot_path.is_file():
            screenshot_path = None

        test_name = run_data.get("test_name") or run_data.get("collection") or run_json_path.parent.parent.name
        group_name = str(run_data.get("test_group") or report_dir.name or test_name).strip()
        description = str(run_data.get("description") or "").strip()
        likeness = result_data.get("likeness_percent")
        status = result_data.get("status", "pass" if not result_data else "fail")
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
                "index_href": index_path.relative_to(output_path.parent).as_posix() if index_path else "",
                "thumbnail_src": screenshot_path.relative_to(output_path.parent).as_posix() if screenshot_path else "",
            }
        )

    grouped_tests = []
    for group_name in sorted(groups.keys(), key=lambda value: value.lower()):
        group = groups[group_name]
        group["tests"].sort(key=lambda item: (item["platform"].lower(), item["name"].lower(), item["index_href"].lower()))
        grouped_tests.append(group)

    return grouped_tests


def collect_platform_counts(groups: list[dict]) -> list[dict]:
    counts: dict[str, dict[str, int]] = {}

    for group in groups:
        for test in group["tests"]:
            platform = test["platform"].strip()
            platform_counts = counts.setdefault(platform, {"pass": 0, "fail": 0})
            if test["status"] == "pass":
                platform_counts["pass"] += 1
            else:
                platform_counts["fail"] += 1

    rows: list[dict] = []
    for platform in sorted((key for key in counts.keys() if key), key=lambda value: value.lower()):
        rows.append(
            {
                "platform": platform,
                "label": platform_label(platform),
                "pass": counts[platform]["pass"],
                "fail": counts[platform]["fail"],
            }
        )

    rows.append(
        {
            "platform": "__total__",
            "label": "total",
            "pass": sum(bucket["pass"] for bucket in counts.values()),
            "fail": sum(bucket["fail"] for bucket in counts.values()),
        }
    )

    return rows


def render_html(title: str, subtitle: str, groups: list[dict], platform_rows: list[dict], overall_status_text: str, overall_status_class: str) -> str:
    rows = []
    for group_index, group in enumerate(groups):
        thumbs = []
        for test_index, test in enumerate(group["tests"]):
            thumbnail = (
                f'<img src="{escape(test["thumbnail_src"])}" alt="{escape(test["name"])} thumbnail">'
                if test["thumbnail_src"]
                else '<div class="missing-thumb">No screenshot</div>'
            )
            thumb_platform_icon_name = platform_icon_name(test["platform"])
            platform_icon_html = (
                inline_svg_html(thumb_platform_icon_name, prefix=f"thumb-{group_index}-{test_index}-")
                if thumb_platform_icon_name
                else ""
            )
            thumbs.append(
                f"""<a class="thumb-card {escape(test["status"])}" href="{escape(test["index_href"])}">
  <div class="thumb">{thumbnail}</div>
  <div class="thumb-caption">
    <span class="thumb-status">{escape(test["status_icon"])} {escape(test["likeness_text"])}</span>
    <span class="thumb-platform">{platform_icon_html}{escape(test["platform"])}</span>
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

    platform_rows_html = "\n".join(
        f"""<tr class="{escape('total-row' if row['platform'] == '__total__' else 'platform-row')}">
      <td>{platform_display_html(row["platform"], row["label"], prefix=f'platform-{index}-')}</td>
      <td>{row["pass"]}</td>
      <td>{row["fail"]}</td>
    </tr>"""
        for index, row in enumerate(platform_rows)
    )

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
    .summary-row {{
      display: flex;
      align-items: flex-start;
      gap: 16px;
      margin-bottom: 24px;
    }}
    .totals {{
      width: 100%;
      max-width: 640px;
      border-collapse: collapse;
    }}
    .totals th,
    .totals td {{
      padding: 10px 14px;
      background: #182028;
      border: 1px solid #2b3947;
      text-align: left;
      font-size: 14px;
    }}
    .totals th {{
      color: #9fb0c0;
      font-weight: 600;
    }}
    .totals td {{
      color: #d7e0ea;
    }}
    .platform-label {{
      display: inline-flex;
      align-items: center;
      gap: 6px;
    }}
    .platform-label .inline-icon {{
      width: 14px;
      height: 14px;
      min-width: 14px;
      min-height: 14px;
    }}
    .totals tbody tr:last-child td {{
      font-weight: 700;
    }}
    .totals tbody tr.total-row td {{
      background: #1e2832;
    }}
    .overall-status {{
      flex: 0 0 auto;
      padding: 10px 14px;
      border-radius: 8px;
      border: 1px solid #2b3947;
      font-size: 14px;
      font-weight: 700;
      white-space: nowrap;
      margin-top: 0;
    }}
    .overall-status.pass {{
      color: #6ee7a0;
      background: rgba(110, 231, 160, 0.08);
      border-color: rgba(110, 231, 160, 0.28);
    }}
    .overall-status.fail {{
      color: #ff7b7b;
      background: rgba(255, 123, 123, 0.08);
      border-color: rgba(255, 123, 123, 0.28);
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
      display: inline-flex;
      align-items: center;
      gap: 4px;
    }}
  </style>
</head>
<body>
  <h1>{escape(title)}</h1>
  {f'<p class="subtitle">{escape(subtitle)}</p>' if subtitle else ""}
  <div class="summary-row">
    <table class="totals">
      <thead>
        <tr>
          <th>platform</th>
          <th>passed</th>
          <th>failed</th>
        </tr>
      </thead>
      <tbody>
        {platform_rows_html}
      </tbody>
    </table>
    <div class="overall-status {overall_status_class}">{overall_status_text}</div>
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
    platform_rows = collect_platform_counts(groups)
    tests = [test for group in groups for test in group["tests"]]
    pass_count = sum(1 for test in tests if test["status"] == "pass")
    fail_count = sum(1 for test in tests if test["status"] != "pass")
    overall_status_text = f"{FAIL_ICON} FAIL" if fail_count > 0 else f"{PASS_ICON} PASS"
    overall_status_class = "fail" if fail_count > 0 else "pass"

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        render_html(args.title, args.subtitle, groups, platform_rows, overall_status_text, overall_status_class),
        encoding="utf8",
    )
    total_count = len(tests)

    summary_json_path = Path(args.summary_json).resolve()
    summary_json_path.parent.mkdir(parents=True, exist_ok=True)
    summary_json_path.write_text(
        json.dumps(
            {
                "pass_count": pass_count,
                "fail_count": fail_count,
                "total_count": total_count,
                "platform_counts": platform_rows,
            },
            indent=2,
        )
        + "\n",
        encoding="utf8",
    )

    print("platform | passed | failed")
    for row in platform_rows:
        print(f"{row['label']} | {row['pass']} | {row['fail']}")
    print(f"Wrote summary: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
