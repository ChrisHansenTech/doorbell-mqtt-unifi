#!/usr/bin/env python3
"""
Aggregate GCC gcov line, function, and branch coverage for first-party src/*.c files.

Generates:
  - Terminal summary table (line, function, and branch coverage)
  - LCOV-compatible tracefile (coverage/lcov.info)
  - Self-contained detailed HTML report (coverage/html/index.html + per-file pages)
  - Machine-readable JSON summary (coverage/coverage.json)

Requires only Python 3 standard library and `gcov` (GCC 9+ JSON output).
Includes all first-party src/*.c files (even those with 0% coverage) and excludes
third-party libraries (src/third_party/), test code (tests/), mocks, and headers.
"""

import argparse
import html
import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


def normalize_rel_path(fname: str, repo_root: Path) -> str:
    try:
        return Path(os.path.relpath(os.path.abspath(os.path.join(repo_root, fname)), repo_root)).as_posix()
    except ValueError:
        return fname


def is_first_party_app_source(rel_path: str) -> bool:
    return (
        rel_path.startswith("src/")
        and not rel_path.startswith("src/third_party/")
        and rel_path.endswith(".c")
    )


def collect_coverage(cov_obj_dir: Path, repo_root: Path):
    """Collect line, function, and branch coverage across all first-party src/*.c files."""
    # Per-file maps:
    #   lines[rel]     -> {line_no: count}
    #   branches[rel]  -> {(line_no, branch_idx): count}
    #   functions[rel] -> {func_name: {"start_line": int, "end_line": int, "count": int}}
    file_lines = {}
    file_branches = {}
    file_functions = {}

    # Seed every first-party src/*.c file so files with no .gcno/.gcda still appear
    first_party_sources = sorted(
        p.relative_to(repo_root).as_posix()
        for p in (repo_root / "src").glob("*.c")
    )
    for rel in first_party_sources:
        file_lines[rel] = {}
        file_branches[rel] = {}
        file_functions[rel] = {}

    gcno_files = sorted((cov_obj_dir / "src").glob("*.gcno"))
    if not gcno_files:
        print(f"ERROR: No .gcno files found in {cov_obj_dir / 'src'}", file=sys.stderr)
        return None

    for gcno in gcno_files:
        cmd = ["gcov", "-b", "-c", "-j", "-t", str(gcno)]
        proc = subprocess.run(cmd, cwd=repo_root, capture_output=True, text=True)
        if proc.returncode != 0:
            print(f"WARNING: gcov failed for {gcno}: {proc.stderr.strip()}", file=sys.stderr)
            continue

        for raw_line in proc.stdout.splitlines():
            raw_line = raw_line.strip()
            if not raw_line or not raw_line.startswith("{"):
                continue
            data = json.loads(raw_line)
            for fobj in data.get("files", []):
                rel = normalize_rel_path(fobj.get("file", ""), repo_root)
                if not is_first_party_app_source(rel):
                    continue

                lmap = file_lines.setdefault(rel, {})
                bmap = file_branches.setdefault(rel, {})
                fmap = file_functions.setdefault(rel, {})

                for fn in fobj.get("functions", []):
                    name = fn.get("name", "")
                    if not name:
                        continue
                    cnt = int(fn.get("execution_count", 0))
                    start_line = int(fn.get("start_line", 0))
                    end_line = int(fn.get("end_line", start_line))
                    prev = fmap.get(name)
                    if prev is None:
                        fmap[name] = {
                            "start_line": start_line,
                            "end_line": end_line,
                            "count": cnt,
                        }
                    else:
                        prev["count"] = max(prev["count"], cnt)

                for line in fobj.get("lines", []):
                    ln = int(line["line_number"])
                    cnt = int(line.get("count", 0))
                    lmap[ln] = max(lmap.get(ln, 0), cnt)

                    for b_idx, br in enumerate(line.get("branches", [])):
                        bkey = (ln, b_idx)
                        bcnt = int(br.get("count", 0))
                        bmap[bkey] = max(bmap.get(bkey, 0), bcnt)

    return file_lines, file_functions, file_branches


def calc_pct(covered: int, total: int):
    if total == 0:
        return None
    return covered * 100.0 / total


def fmt_pct(covered: int, total: int) -> str:
    pct = calc_pct(covered, total)
    if pct is None:
        return "  N/A "
    return f"{pct:5.1f}%"


def pct_class(covered: int, total: int) -> str:
    pct = calc_pct(covered, total)
    if pct is None:
        return "na"
    if pct >= 85.0:
        return "high"
    if pct >= 70.0:
        return "med"
    return "low"


def build_metrics(file_lines, file_functions, file_branches):
    records = []
    totals = {
        "lines_covered": 0,
        "lines_total": 0,
        "functions_covered": 0,
        "functions_total": 0,
        "branches_covered": 0,
        "branches_total": 0,
    }

    for rel in sorted(file_lines.keys()):
        lmap = file_lines[rel]
        fmap = file_functions.get(rel, {})
        bmap = file_branches.get(rel, {})

        l_all = len(lmap)
        l_cov = sum(1 for c in lmap.values() if c > 0)
        f_all = len(fmap)
        f_cov = sum(1 for info in fmap.values() if info["count"] > 0)
        b_all = len(bmap)
        b_cov = sum(1 for c in bmap.values() if c > 0)

        uncovered_lines = sorted(ln for ln, c in lmap.items() if c == 0)
        uncovered_funcs = sorted(
            (info["start_line"], name)
            for name, info in fmap.items()
            if info["count"] == 0
        )

        rec = {
            "file": rel,
            "lines_covered": l_cov,
            "lines_total": l_all,
            "line_pct": round(calc_pct(l_cov, l_all), 2) if l_all else 0.0,
            "functions_covered": f_cov,
            "functions_total": f_all,
            "function_pct": round(calc_pct(f_cov, f_all), 2) if f_all else 0.0,
            "branches_covered": b_cov,
            "branches_total": b_all,
            "branch_pct": round(calc_pct(b_cov, b_all), 2) if b_all else 0.0,
            "uncovered_lines": uncovered_lines,
            "uncovered_functions": [{"name": name, "line": ln} for ln, name in uncovered_funcs],
        }
        records.append(rec)

        totals["lines_covered"] += l_cov
        totals["lines_total"] += l_all
        totals["functions_covered"] += f_cov
        totals["functions_total"] += f_all
        totals["branches_covered"] += b_cov
        totals["branches_total"] += b_all

    totals["line_pct"] = round(calc_pct(totals["lines_covered"], totals["lines_total"]) or 0.0, 2)
    totals["function_pct"] = round(calc_pct(totals["functions_covered"], totals["functions_total"]) or 0.0, 2)
    totals["branch_pct"] = round(calc_pct(totals["branches_covered"], totals["branches_total"]) or 0.0, 2)
    return records, totals


def print_terminal_summary(records, totals):
    width = 108
    print()
    print("Coverage Summary (First-Party src/*.c; excludes src/third_party/, tests/, mocks/, headers)")
    print("=" * width)
    print(
        f"{'File':<30} {'Lines':>11} {'Line %':>7}   "
        f"{'Funcs':>9} {'Func %':>7}   {'Branches':>11} {'Branch %':>8}"
    )
    print("-" * width)
    for r in records:
        l_str = f"{r['lines_covered']}/{r['lines_total']}"
        f_str = f"{r['functions_covered']}/{r['functions_total']}"
        b_str = f"{r['branches_covered']}/{r['branches_total']}"
        print(
            f"{r['file']:<30} {l_str:>11} {fmt_pct(r['lines_covered'], r['lines_total']):>7}   "
            f"{f_str:>9} {fmt_pct(r['functions_covered'], r['functions_total']):>7}   "
            f"{b_str:>11} {fmt_pct(r['branches_covered'], r['branches_total']):>8}"
        )
    print("-" * width)
    l_str = f"{totals['lines_covered']}/{totals['lines_total']}"
    f_str = f"{totals['functions_covered']}/{totals['functions_total']}"
    b_str = f"{totals['branches_covered']}/{totals['branches_total']}"
    print(
        f"{'TOTAL':<30} {l_str:>11} {fmt_pct(totals['lines_covered'], totals['lines_total']):>7}   "
        f"{f_str:>9} {fmt_pct(totals['functions_covered'], totals['functions_total']):>7}   "
        f"{b_str:>11} {fmt_pct(totals['branches_covered'], totals['branches_total']):>8}"
    )
    print("=" * width)


def write_lcov(lcov_path: Path, file_lines, file_functions, file_branches):
    lcov_path.parent.mkdir(parents=True, exist_ok=True)
    out = []
    for rel in sorted(file_lines.keys()):
        lmap = file_lines[rel]
        fmap = file_functions.get(rel, {})
        bmap = file_branches.get(rel, {})

        out.append("TN:")
        out.append(f"SF:{rel}")

        sorted_funcs = sorted(fmap.items(), key=lambda kv: (kv[1]["start_line"], kv[0]))
        for name, info in sorted_funcs:
            out.append(f"FN:{info['start_line']},{name}")
        for name, info in sorted_funcs:
            out.append(f"FNDA:{info['count']},{name}")
        out.append(f"FNF:{len(sorted_funcs)}")
        out.append(f"FNH:{sum(1 for _, info in sorted_funcs if info['count'] > 0)}")

        for ln in sorted(lmap.keys()):
            out.append(f"DA:{ln},{lmap[ln]}")
        out.append(f"LF:{len(lmap)}")
        out.append(f"LH:{sum(1 for c in lmap.values() if c > 0)}")

        for (ln, b_idx) in sorted(bmap.keys()):
            bcnt = bmap[(ln, b_idx)]
            line_hits = lmap.get(ln, 0)
            taken = str(bcnt) if line_hits > 0 else "-"
            out.append(f"BRDA:{ln},0,{b_idx},{taken}")
        out.append(f"BRF:{len(bmap)}")
        out.append(f"BRH:{sum(1 for c in bmap.values() if c > 0)}")
        out.append("end_of_record")

    lcov_path.write_text("\n".join(out) + "\n", encoding="utf-8")


CSS_STYLES = """
:root {
  --bg: #0f172a;
  --panel: #1e293b;
  --border: #334155;
  --text: #e2e8f0;
  --muted: #94a3b8;
  --high: #22c55e;
  --high-bg: rgba(34, 197, 94, 0.14);
  --med: #eab308;
  --med-bg: rgba(234, 179, 8, 0.16);
  --low: #ef4444;
  --low-bg: rgba(239, 68, 68, 0.16);
  --link: #38bdf8;
}
* { box-sizing: border-box; }
body {
  margin: 0; padding: 24px; background: var(--bg); color: var(--text);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  line-height: 1.5;
}
a { color: var(--link); text-decoration: none; }
a:hover { text-decoration: underline; }
h1, h2 { margin: 0 0 12px 0; font-weight: 600; }
.meta { color: var(--muted); font-size: 0.9rem; margin-bottom: 20px; }
.cards { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 16px; margin-bottom: 24px; }
.card {
  background: var(--panel); border: 1px solid var(--border); border-radius: 10px; padding: 16px;
}
.card .label { color: var(--muted); font-size: 0.85rem; text-transform: uppercase; letter-spacing: 0.04em; }
.card .value { font-size: 1.9rem; font-weight: 700; margin: 4px 0; }
.card .detail { color: var(--muted); font-size: 0.9rem; }
.bar { height: 8px; background: #0b1220; border-radius: 4px; overflow: hidden; margin-top: 10px; }
.bar > span { display: block; height: 100%; }
.high { color: var(--high); }
.med { color: var(--med); }
.low { color: var(--low); }
.na { color: var(--muted); }
.fill-high { background: var(--high); }
.fill-med { background: var(--med); }
.fill-low { background: var(--low); }
.fill-na { background: var(--muted); }
table {
  width: 100%; border-collapse: collapse; background: var(--panel);
  border: 1px solid var(--border); border-radius: 10px; overflow: hidden; margin-bottom: 24px;
}
th, td { padding: 10px 12px; border-bottom: 1px solid var(--border); text-align: left; font-size: 0.92rem; }
th { background: #162033; color: var(--muted); font-weight: 600; }
td.num, th.num { text-align: right; font-variant-numeric: tabular-nums; }
tr:last-child td { border-bottom: none; }
tr.total-row td { font-weight: 700; background: #162033; }
.code-wrap {
  background: var(--panel); border: 1px solid var(--border); border-radius: 10px;
  overflow-x: auto; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: 13px;
}
.code-table { width: 100%; border-collapse: collapse; border: none; margin: 0; }
.code-table td { padding: 1px 10px; border: none; vertical-align: top; white-space: pre; }
.code-table td.ln { width: 52px; text-align: right; color: var(--muted); user-select: none; border-right: 1px solid var(--border); }
.code-table td.hits { width: 64px; text-align: right; color: var(--muted); user-select: none; border-right: 1px solid var(--border); }
.code-table td.br { width: 68px; text-align: center; font-size: 11px; user-select: none; border-right: 1px solid var(--border); }
.line-hit td.src, .line-hit td.hits { background: var(--high-bg); }
.line-miss td.src, .line-miss td.hits { background: var(--low-bg); }
.line-partial td.br { background: var(--med-bg); color: var(--med); font-weight: 600; }
.line-br-hit td.br { color: var(--high); }
.line-br-miss td.br { background: var(--low-bg); color: var(--low); font-weight: 600; }
.badge { display: inline-block; padding: 2px 8px; border-radius: 999px; font-size: 0.78rem; font-weight: 600; }
.badge-high { background: var(--high-bg); color: var(--high); }
.badge-low { background: var(--low-bg); color: var(--low); }
"""


def html_slug(rel_path: str) -> str:
    return rel_path.replace("/", "_") + ".html"


def render_metric_card(title: str, covered: int, total: int) -> str:
    cls = pct_class(covered, total)
    pct = calc_pct(covered, total)
    pct_text = f"{pct:.1f}%" if pct is not None else "N/A"
    width = pct if pct is not None else 0.0
    return (
        f'<div class="card">'
        f'<div class="label">{html.escape(title)}</div>'
        f'<div class="value {cls}">{pct_text}</div>'
        f'<div class="detail">{covered} / {total}</div>'
        f'<div class="bar"><span class="fill-{cls}" style="width: {width:.1f}%"></span></div>'
        f'</div>'
    )


def write_html_report(html_dir: Path, repo_root: Path, records, totals, file_lines, file_functions, file_branches):
    html_dir.mkdir(parents=True, exist_ok=True)
    generated_at = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")

    # Index page
    rows_html = []
    for r in records:
        l_cls = pct_class(r["lines_covered"], r["lines_total"])
        f_cls = pct_class(r["functions_covered"], r["functions_total"])
        b_cls = pct_class(r["branches_covered"], r["branches_total"])
        slug = html_slug(r["file"])
        rows_html.append(
            f"<tr>"
            f'<td><a href="{html.escape(slug)}">{html.escape(r["file"])}</a></td>'
            f'<td class="num">{r["lines_covered"]}/{r["lines_total"]}</td>'
            f'<td class="num {l_cls}">{fmt_pct(r["lines_covered"], r["lines_total"]).strip()}</td>'
            f'<td class="num">{r["functions_covered"]}/{r["functions_total"]}</td>'
            f'<td class="num {f_cls}">{fmt_pct(r["functions_covered"], r["functions_total"]).strip()}</td>'
            f'<td class="num">{r["branches_covered"]}/{r["branches_total"]}</td>'
            f'<td class="num {b_cls}">{fmt_pct(r["branches_covered"], r["branches_total"]).strip()}</td>'
            f"</tr>"
        )

    index_html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>doorbell-mqtt-unifi — Code Coverage Report</title>
  <style>{CSS_STYLES}</style>
</head>
<body>
  <h1>doorbell-mqtt-unifi — Code Coverage Report</h1>
  <div class="meta">Generated {generated_at} &middot; Scope: First-party <code>src/*.c</code> (excludes <code>src/third_party/</code>, <code>tests/</code>, mocks, and headers)</div>
  <div class="cards">
    {render_metric_card("Line Coverage", totals["lines_covered"], totals["lines_total"])}
    {render_metric_card("Function Coverage", totals["functions_covered"], totals["functions_total"])}
    {render_metric_card("Branch Coverage", totals["branches_covered"], totals["branches_total"])}
  </div>
  <h2>Coverage by Application Source File</h2>
  <table>
    <thead>
      <tr>
        <th>Source File</th>
        <th class="num">Lines</th>
        <th class="num">Line %</th>
        <th class="num">Functions</th>
        <th class="num">Function %</th>
        <th class="num">Branches</th>
        <th class="num">Branch %</th>
      </tr>
    </thead>
    <tbody>
      {"".join(rows_html)}
      <tr class="total-row">
        <td>TOTAL ({len(records)} files)</td>
        <td class="num">{totals["lines_covered"]}/{totals["lines_total"]}</td>
        <td class="num {pct_class(totals['lines_covered'], totals['lines_total'])}">{fmt_pct(totals["lines_covered"], totals["lines_total"]).strip()}</td>
        <td class="num">{totals["functions_covered"]}/{totals["functions_total"]}</td>
        <td class="num {pct_class(totals['functions_covered'], totals['functions_total'])}">{fmt_pct(totals["functions_covered"], totals["functions_total"]).strip()}</td>
        <td class="num">{totals["branches_covered"]}/{totals["branches_total"]}</td>
        <td class="num {pct_class(totals['branches_covered'], totals['branches_total'])}">{fmt_pct(totals["branches_covered"], totals["branches_total"]).strip()}</td>
      </tr>
    </tbody>
  </table>
</body>
</html>
"""
    (html_dir / "index.html").write_text(index_html, encoding="utf-8")

    # Per-file detail pages
    for r in records:
        rel = r["file"]
        lmap = file_lines.get(rel, {})
        fmap = file_functions.get(rel, {})
        bmap = file_branches.get(rel, {})

        # Group branches by line
        branches_by_line = {}
        for (ln, b_idx), bcnt in bmap.items():
            branches_by_line.setdefault(ln, []).append((b_idx, bcnt))

        src_path = repo_root / rel
        src_lines = src_path.read_text(encoding="utf-8", errors="replace").splitlines() if src_path.exists() else []

        code_rows = []
        for idx, text in enumerate(src_lines, start=1):
            classes = []
            hits_str = ""
            if idx in lmap:
                hits = lmap[idx]
                hits_str = str(hits)
                classes.append("line-hit" if hits > 0 else "line-miss")

            br_str = ""
            if idx in branches_by_line:
                blist = branches_by_line[idx]
                btot = len(blist)
                bcov = sum(1 for _, c in blist if c > 0)
                br_str = f"{bcov}/{btot}"
                if bcov == btot:
                    classes.append("line-br-hit")
                elif bcov == 0:
                    classes.append("line-br-miss")
                else:
                    classes.append("line-partial")

            cls_attr = f' class="{" ".join(classes)}"' if classes else ""
            code_rows.append(
                f'<tr id="L{idx}"{cls_attr}>'
                f'<td class="ln"><a href="#L{idx}">{idx}</a></td>'
                f'<td class="hits">{hits_str}</td>'
                f'<td class="br">{br_str}</td>'
                f'<td class="src">{html.escape(text)}</td>'
                f"</tr>"
            )

        func_rows = []
        for name, info in sorted(fmap.items(), key=lambda kv: (kv[1]["start_line"], kv[0])):
            hit = info["count"]
            badge = '<span class="badge badge-high">Covered</span>' if hit > 0 else '<span class="badge badge-low">Uncovered</span>'
            func_rows.append(
                f"<tr>"
                f'<td><a href="#L{info["start_line"]}"><code>{html.escape(name)}</code></a></td>'
                f'<td class="num">{info["start_line"]}</td>'
                f'<td class="num">{hit}</td>'
                f"<td>{badge}</td>"
                f"</tr>"
            )

        file_html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>{html.escape(rel)} — Coverage</title>
  <style>{CSS_STYLES}</style>
</head>
<body>
  <div class="meta"><a href="index.html">&larr; Back to Coverage Summary</a> &middot; Generated {generated_at}</div>
  <h1><code>{html.escape(rel)}</code></h1>
  <div class="cards">
    {render_metric_card("Lines", r["lines_covered"], r["lines_total"])}
    {render_metric_card("Functions", r["functions_covered"], r["functions_total"])}
    {render_metric_card("Branches", r["branches_covered"], r["branches_total"])}
  </div>
  <h2>Functions ({r["functions_covered"]}/{r["functions_total"]})</h2>
  <table>
    <thead>
      <tr><th>Function</th><th class="num">Line</th><th class="num">Calls</th><th>Status</th></tr>
    </thead>
    <tbody>
      {"".join(func_rows) if func_rows else '<tr><td colspan="4" class="na">No functions recorded</td></tr>'}
    </tbody>
  </table>
  <h2>Annotated Source</h2>
  <div class="code-wrap">
    <table class="code-table">
      <tbody>
        {"".join(code_rows)}
      </tbody>
    </table>
  </div>
</body>
</html>
"""
        (html_dir / html_slug(rel)).write_text(file_html, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="Generate coverage reports from GCC gcov data.")
    parser.add_argument("cov_obj_dir", nargs="?", default="build-cov", help="Directory containing coverage .gcno/.gcda files")
    parser.add_argument("--lcov", dest="lcov_path", default=None, help="Output path for LCOV tracefile (e.g. coverage/lcov.info)")
    parser.add_argument("--html", dest="html_dir", default=None, help="Output directory for HTML report (e.g. coverage/html)")
    parser.add_argument("--json", dest="json_path", default=None, help="Output path for JSON summary (e.g. coverage/coverage.json)")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    cov_obj_dir = Path(args.cov_obj_dir)
    if not cov_obj_dir.is_absolute():
        cov_obj_dir = (repo_root / cov_obj_dir).resolve()

    collected = collect_coverage(cov_obj_dir, repo_root)
    if collected is None:
        return 1

    file_lines, file_functions, file_branches = collected
    records, totals = build_metrics(file_lines, file_functions, file_branches)

    print_terminal_summary(records, totals)

    if args.lcov_path:
        lcov_path = Path(args.lcov_path)
        if not lcov_path.is_absolute():
            lcov_path = repo_root / lcov_path
        write_lcov(lcov_path, file_lines, file_functions, file_branches)
        print(f"LCOV report written to: {lcov_path.relative_to(repo_root)}")

    if args.html_dir:
        html_dir = Path(args.html_dir)
        if not html_dir.is_absolute():
            html_dir = repo_root / html_dir
        write_html_report(html_dir, repo_root, records, totals, file_lines, file_functions, file_branches)
        print(f"HTML report written to: {(html_dir / 'index.html').relative_to(repo_root)}")

    if args.json_path:
        json_path = Path(args.json_path)
        if not json_path.is_absolute():
            json_path = repo_root / json_path
        json_path.parent.mkdir(parents=True, exist_ok=True)
        payload = {"files": records, "total": totals}
        json_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        print(f"JSON report written to: {json_path.relative_to(repo_root)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
