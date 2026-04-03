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
        default="build/render-test",
        help="Root directory containing per-test report folders. Default: build/render-test",
    )
    parser.add_argument(
        "--output",
        default="build/render-test/index.html",
        help="Output HTML summary path. Default: build/render-test/index.html",
    )
    parser.add_argument(
        "--summary-json",
        default="build/render-test/summary.json",
        help="JSON file to write aggregate summary data to. Default: build/render-test/summary.json",
    )
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf8") as fh:
        return json.load(fh)


def collect_tests(reports_root: Path, output_path: Path) -> list[dict]:
    tests: list[dict] = []

    for run_json_path in sorted(reports_root.glob("*/report/run.json")):
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
        likeness = result_data.get("likeness_percent")
        status = result_data.get("status", "fail")

        tests.append(
            {
                "name": test_name,
                "collection": run_data.get("collection", ""),
                "status": status,
                "status_icon": PASS_ICON if status == "pass" else FAIL_ICON,
                "likeness": likeness,
                "likeness_text": f"{likeness:.2f}%" if isinstance(likeness, (int, float)) else "n/a",
                "index_href": index_path.relative_to(output_path.parent).as_posix(),
                "thumbnail_src": screenshot_path.relative_to(output_path.parent).as_posix() if screenshot_path else "",
            }
        )

    tests.sort(key=lambda item: item["name"].lower())
    return tests


def render_html(title: str, subtitle: str, tests: list[dict]) -> str:
    pass_count = sum(1 for test in tests if test["status"] == "pass")
    fail_count = sum(1 for test in tests if test["status"] != "pass")

    cards = []
    for test in tests:
        thumbnail = (
            f'<img src="{escape(test["thumbnail_src"])}" alt="{escape(test["name"])} thumbnail">'
            if test["thumbnail_src"]
            else '<div class="missing-thumb">No screenshot</div>'
        )
        cards.append(
            f"""<article class="card {escape(test["status"])}">
  <a class="card-link" href="{escape(test["index_href"])}">
    <div class="thumb">{thumbnail}</div>
    <h2>{escape(test["name"])}</h2>
  </a>
  <p class="score">{escape(test["status_icon"])} {escape(test["likeness_text"])} likeness</p>
  <p class="collection">{escape(test["collection"])}</p>
</article>"""
        )

    cards_html = "\n".join(cards) if cards else '<p class="empty">No test reports found.</p>'

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
    .grid {{
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
      gap: 20px;
    }}
    .card {{
      background: #182028;
      border-radius: 10px;
      overflow: hidden;
      border: 1px solid #2b3947;
    }}
    .card.pass {{
      box-shadow: inset 0 0 0 1px rgba(90, 180, 110, 0.35);
    }}
    .card.fail {{
      box-shadow: inset 0 0 0 1px rgba(220, 80, 80, 0.35);
    }}
    .card-link {{
      color: inherit;
      text-decoration: none;
      display: block;
    }}
    .thumb {{
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
    }}
    .missing-thumb {{
      color: #8fa1b2;
      font-size: 14px;
    }}
    h2 {{
      font-size: 18px;
      margin: 14px 16px 8px;
    }}
    .score, .collection, .empty {{
      margin: 0 16px 14px;
    }}
    .score {{
      font-weight: 600;
    }}
    .collection {{
      color: #9fb0c0;
      font-size: 13px;
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
  <section class="grid">
    {cards_html}
  </section>
</body>
</html>
"""


def main() -> int:
    args = parse_args()
    reports_root = Path(args.reports_root).resolve()
    output_path = Path(args.output).resolve()

    tests = collect_tests(reports_root, output_path)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(render_html(args.title, args.subtitle, tests), encoding="utf8")

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
