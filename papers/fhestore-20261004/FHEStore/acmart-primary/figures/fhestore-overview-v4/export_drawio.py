#!/usr/bin/env python3
"""Render the native drawio model using the official diagrams.net viewer.

Usage: python3 export_drawio.py --viewer-js /path/to/viewer-static.min.js
Requires Chrome/Chromium and rsvg-convert; the drawio file itself is standalone.
"""
import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET


class ResultParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.current = None
        self.results = {}

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "pre" and attrs.get("id", "").startswith("export-"):
            self.current = attrs["id"]
            self.results[self.current] = ""

    def handle_endtag(self, tag):
        if tag == "pre":
            self.current = None

    def handle_data(self, data):
        if self.current:
            self.results[self.current] += data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--viewer-js", type=Path, required=True)
    parser.add_argument("--chrome", default="/opt/google/chrome/chrome")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    folder = Path(__file__).resolve().parent
    source = folder / "fhestore-overview.drawio"
    outputs = {ext: folder / f"fhestore-overview.{ext}" for ext in ("svg", "pdf", "png")}
    if not args.force and any(p.exists() for p in outputs.values()):
        parser.error("Exports exist; use --force to regenerate them")
    renderer = shutil.which("rsvg-convert")
    if not renderer:
        parser.error("rsvg-convert is required")
    xml = source.read_text(encoding="utf-8")
    model = ET.fromstring(xml).find("./diagram/mxGraphModel")
    if model is None:
        parser.error("Expected an uncompressed native drawio model")
    width = int(model.get("pageWidth", "1800"))
    height = int(model.get("pageHeight", "900"))
    payload = json.dumps(xml).replace("</", "<\\/")
    html = """<!doctype html><html><head><meta charset="utf-8">
<style>body{margin:0}#graph{width:Wpx;height:Hpx}</style>
<script>var mxLoadResources=false;var mxLoadStylesheets=false;</script>
<script src="VIEWER"></script></head><body><div id="graph"></div>
<pre id="export-svg"></pre><pre id="export-checks"></pre><pre id="export-error"></pre>
<script>
try {
  const doc = mxUtils.parseXml(PAYLOAD);
  const node = doc.getElementsByTagName('mxGraphModel')[0];
  const graph = new Graph(document.getElementById('graph'));
  graph.setEnabled(false);
  graph.setHtmlLabels(false);
  // Graph normally treats wrapping labels as HTML even when html=0.
  // These cells have explicit newlines and native plain SVG text labels.
  graph.isHtmlLabel = function(cell) {
    return mxUtils.getValue(this.getCurrentCellStyle(cell), 'html', '0') === '1';
  };
  new mxCodec(doc).decode(node, graph.getModel());
  graph.getView().validate();
  const ns = 'http://www.w3.org/2000/svg';
  const svg = document.createElementNS(ns,'svg');
  svg.setAttribute('xmlns',ns);
  svg.setAttribute('version','1.1');
  svg.setAttribute('width','178mm');
  svg.setAttribute('height', String(178*H/W)+'mm');
  svg.setAttribute('viewBox','0 0 W H');
  const bg = document.createElementNS(ns,'rect');
  bg.setAttribute('width','W');bg.setAttribute('height','H');bg.setAttribute('fill','#fff');
  svg.appendChild(bg);
  const canvas = new mxSvgCanvas2D(svg);
  canvas.foEnabled = false;
  const exporter = new mxImageExport();
  const parent = graph.getDefaultParent();
  const checks = {vertices:0,edges:0,unresolvedEdges:[],unrenderedCells:[]};
  const all = graph.getModel().cells;
  for (const id in all) {
    const cell = all[id];
    if (cell.vertex) checks.vertices++;
    if (cell.edge) {
      checks.edges++;
      if ((!cell.source || !cell.target) && !id.startsWith('legend-')) checks.unresolvedEdges.push(id);
    }
    if ((cell.vertex || cell.edge) && !graph.getView().getState(cell)) checks.unrenderedCells.push(id);
  }
  for (let i=0;i<graph.getModel().getChildCount(parent);i++) {
    exporter.drawState(graph.getView().getState(graph.getModel().getChildAt(parent,i)),canvas);
  }
  checks.textElements = svg.getElementsByTagName('text').length;
  checks.rasterElements = svg.getElementsByTagName('image').length;
  checks.foreignObjects = svg.getElementsByTagName('foreignObject').length;
  document.getElementById('export-svg').textContent = new XMLSerializer().serializeToString(svg);
  document.getElementById('export-checks').textContent = JSON.stringify(checks);
} catch (e) { document.getElementById('export-error').textContent = e.stack || String(e); }
</script></body></html>"""
    html = html.replace("VIEWER", args.viewer_js.resolve().as_uri()).replace("PAYLOAD", payload)
    html = html.replace("Wpx", f"{width}px").replace("Hpx", f"{height}px")
    html = html.replace("'0 0 W H'", f"'0 0 {width} {height}'")
    html = html.replace("'W'", f"'{width}'").replace("'H'", f"'{height}'")
    html = html.replace("178*H/W", f"178*{height}/{width}")
    with tempfile.TemporaryDirectory(prefix="fhestore-drawio-export-") as scratch:
        page = Path(scratch) / "render.html"
        page.write_text(html, encoding="utf-8")
        result = subprocess.run([
            args.chrome, "--headless", "--no-sandbox", "--disable-gpu",
            "--disable-dev-shm-usage", "--disable-background-networking",
            "--allow-file-access-from-files", "--no-first-run",
            f"--user-data-dir={scratch}/profile", "--virtual-time-budget=2000",
            "--dump-dom", page.as_uri(),
        ], capture_output=True, text=True, timeout=45)
        parsed = ResultParser()
        parsed.feed(result.stdout)
        if parsed.results.get("export-error"):
            raise RuntimeError(parsed.results["export-error"])
        rendered = parsed.results.get("export-svg", "")
        if not rendered:
            raise RuntimeError(f"Chrome did not produce SVG: {result.stderr[-2000:]}")
    checks = json.loads(parsed.results["export-checks"])
    if checks["unresolvedEdges"] or checks["unrenderedCells"] or checks["rasterElements"] or checks["foreignObjects"]:
        raise RuntimeError(f"Native model rendering checks failed: {checks}")
    ET.fromstring(rendered)
    outputs["svg"].write_text(rendered + "\n", encoding="utf-8")
    subprocess.run([renderer,"-f","pdf","-o",str(outputs["pdf"]),str(outputs["svg"])], check=True)
    subprocess.run([renderer,"-f","png","-w","3600","-h",str(round(3600*height/width)),
                    "-o",str(outputs["png"]),str(outputs["svg"])], check=True)
    checks["renderer"] = "Official diagrams.net viewer-static.min.js + mxCodec/mxImageExport"
    checks["viewerSha256"] = hashlib.sha256(args.viewer_js.read_bytes()).hexdigest()
    (folder / "render-checks.json").write_text(json.dumps(checks, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(checks, indent=2))
    for path in outputs.values():
        print(path)


if __name__ == "__main__":
    main()
