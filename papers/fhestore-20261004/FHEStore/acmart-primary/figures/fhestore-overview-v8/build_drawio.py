#!/usr/bin/env python3
"""Native diagrams.net reconstruction of author-selected Figure 1.

Retains the v6 appearance and changes only the author-requested production path.
Run this file, then export_drawio.py with the existing local official viewer.
"""
from pathlib import Path
import hashlib
import json
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
CANVAS = (1089, 370)
PALETTE = {
    "csd_fill": "#D6E3F5", "host_fill": "#F7F8FA", "remote_fill": "#E1D6E7",
    "domain_edge": "#4B5C6B", "remote_edge": "#6D5B7B", "runtime_fill": "#EDF1F5",
    "data": "#315D83", "control": "#596A7A", "pool_fill": "#D6E9F2",
    "pool_edge": "#3C6584", "persistent_fill": "#FFF2CC", "storage_edge": "#778897",
    "accelerator_fill": "#CEE7F2", "accelerator_edge": "#3D6D92",
}
TOPOLOGY = []

def main():
    doc = ET.Element("mxfile",dict(host="app.diagrams.net",type="device",version="24.7.17",compressed="false"))
    diagram = ET.SubElement(doc,"diagram",dict(id="fhestore-overview-v8",name="Author-selected input production"))
    model = ET.SubElement(diagram,"mxGraphModel",dict(dx="1089",dy="370",grid="1",gridSize="1",
        guides="1",tooltips="1",connect="1",arrows="1",fold="1",page="1",pageScale="1",
        pageWidth=str(CANVAS[0]),pageHeight=str(CANVAS[1]),math="0",shadow="0"))
    tree = ET.SubElement(model,"root")
    ET.SubElement(tree,"mxCell",{"id":"0"})
    ET.SubElement(tree,"mxCell",{"id":"1","parent":"0"})
    boxes = {}

    def node(id_, value, x, y, w, h, fill="#FFFFFF", stroke=None, font=20,
             bold=False, shadow=False, dashed=False, shape="rectangle", align="center", lw=2):
        stroke = stroke or PALETTE["domain_edge"]
        style = (f'shape={shape};rounded=0;html=0;whiteSpace=wrap;overflow=visible;'
                 f'fontFamily=Arial;fontSize={font};fontColor=#101010;fontStyle={int(bold)};'
                 f'fillColor={fill};strokeColor={stroke};strokeWidth={lw};'
                 f'shadow={int(shadow)};align={align};verticalAlign=middle;spacing=0;')
        if dashed: style += "dashed=1;dashPattern=4 3;"
        if shape == "cylinder": style += "size=0.12;boundedLbl=1;backgroundOutline=1;"
        c = ET.SubElement(tree,"mxCell",dict(id=id_,value=value,style=style,vertex="1",parent="1"))
        ET.SubElement(c,"mxGeometry",dict(x=str(x),y=str(y),width=str(w),height=str(h),**{"as":"geometry"}))
        boxes[id_] = (x,y,w,h)

    def label(id_, value, x, y, w, h, font=20, bold=False, align="center"):
        node(id_,value,x,y,w,h,"none","none",font,bold,align=align,lw=0)

    def edge(id_, src, dst, x1, y1, x2, y2, kind="control"):
        color = PALETTE["data"] if kind=="data" else PALETTE["control"] if kind=="control" else "#202020"
        lw = 2.5 if kind=="data" else 2 if kind=="control" else 2
        style = (f'edgeStyle=none;rounded=0;html=0;strokeColor={color};strokeWidth={lw};'
                 f'endArrow={"none" if kind=="physical" else "block"};endFill=1;'
                 f'endSize={12 if kind=="data" else 10};startArrow=none;')
        if kind=="control": style += "dashed=1;dashPattern=5 3;"
        attrs = dict(id=id_,value="",style=style,edge="1",parent="1")
        if src and dst:
            sx,sy,sw,sh = boxes[src]
            tx,ty,tw,th = boxes[dst]
            attrs["source"],attrs["target"] = src,dst
            attrs["style"] += (f'exitX={(x1-sx)/sw};exitY={(y1-sy)/sh};exitPerimeter=0;'
                                f'entryX={(x2-tx)/tw};entryY={(y2-ty)/th};entryPerimeter=0;')
        c = ET.SubElement(tree,"mxCell",attrs)
        geo = ET.SubElement(c,"mxGeometry",{"relative":"1","as":"geometry"})
        ET.SubElement(geo,"mxPoint",dict(x=str(x1),y=str(y1),**{"as":"sourcePoint"}))
        ET.SubElement(geo,"mxPoint",dict(x=str(x2),y=str(y2),**{"as":"targetPoint"}))
        if not id_.startswith("legend"):
            TOPOLOGY.append(dict(id=id_,source=src,target=dst,kind=kind,
                                 source_point=[x1,y1],target_point=[x2,y2]))

    # Domain rectangles retain the shallow author-selected headers, fills and shadows.
    node("csd","",11,12,460,316,PALETTE["csd_fill"],shadow=True)
    node("host","",533,12,276,316,PALETTE["host_fill"],shadow=True)
    node("remote","",869,12,205,316,PALETTE["remote_fill"],PALETTE["remote_edge"],shadow=True)
    label("csd-title","FHEStore CSD",19,19,400,34,24,True,"left")
    label("host-title","Host",541,16,245,31,24,True,"left")
    label("remote-title","Remote Server",880,19,184,34,23,True)
    node("storage-data-path","",26,205,430,108,"none",PALETTE["storage_edge"],dashed=True,lw=1.7)
    label("storage-title","Storage-side data path",35,208,290,25,19,True,"left")

    node("device-runtime","Device Runtime",172,111,230,50,PALETTE["runtime_fill"],font=21,bold=True,shadow=True)
    node("application","Application",556,44,230,38,font=21,bold=True,shadow=True)
    node("task-runtime","Task Runtime",556,111,230,50,PALETTE["runtime_fill"],font=21,bold=True,shadow=True)
    node("persistent-data","Persistent\ndata",40,237,106,68,PALETTE["persistent_fill"],PALETTE["storage_edge"],font=20,shadow=True,shape="cylinder",lw=2.2)
    node("operator-pool","Operator Pool",211,236,230,67,PALETTE["pool_fill"],PALETTE["pool_edge"],font=21,bold=True,lw=2.2)
    node("ciphertext-transfer","Ciphertext Transfer",549,204,245,109,"#F8F9FA",PALETTE["storage_edge"],font=20,bold=True,dashed=True,lw=1.7)
    node("server-software","Server-side\nSoftware",887,96,172,64,font=20,shadow=True)
    node("fhe-accelerator","FHE\nAccelerator",884,221,176,65,PALETTE["accelerator_fill"],PALETTE["accelerator_edge"],font=20,lw=2.2,shadow=True)

    # The original PCIe links are undirected physical-boundary connections.
    edge("pcie-upper","csd","host",471,29,533,29,"physical")
    edge("pcie-lower","csd","host",471,313,533,313,"physical")
    label("pcie-label","PCIe",478,42,50,25,20,True)

    # Keep both task and external-controller directions.
    edge("application-submit","application","task-runtime",658,82,658,111)
    edge("application-complete","task-runtime","application",696,111,696,82)
    edge("task-submit","task-runtime","device-runtime",556,128,402,128)
    edge("task-complete","device-runtime","task-runtime",402,144,556,144)
    label("task-submit-label","Submit\ntask",476,89,53,36,14)
    label("task-complete-label","Complete\ntask",473,151,59,36,14)

    # The authorized endpoint repair extends both controls to the actual pool.
    edge("configure-execute","device-runtime","operator-pool",288,161,288,236)
    edge("operator-complete","operator-pool","device-runtime",317,236,317,161)
    label("configure-label","configure / execute",146,171,135,24,14)
    label("operator-complete-label","complete",328,171,74,24,14)
    edge("remote-submit","server-software","fhe-accelerator",958,160,958,221)
    edge("remote-complete","fhe-accelerator","server-software",995,221,995,160)

    # A single forward stream realizes the author's production-only Figure 1.
    edge("storage-to-pool","persistent-data","operator-pool",146,270,211,270,"data")
    edge("pool-to-transfer","operator-pool","ciphertext-transfer",441,270,549,270,"data")
    edge("transfer-to-consumer","ciphertext-transfer","fhe-accelerator",794,270,884,270,"data")
    edge("legend-data",None,None,325,352,377,352,"data")
    label("legend-data-label","Data flow",390,340,134,24,16,align="left")
    edge("legend-control",None,None,548,352,600,352,"control")
    label("legend-control-label","Control flow",614,340,143,24,16,align="left")

    ROOT.mkdir(parents=True,exist_ok=True)
    ET.indent(doc,space="  ")
    ET.ElementTree(doc).write(ROOT/"fhestore-overview.drawio",encoding="utf-8",xml_declaration=True)
    ref = ROOT.parent/"fhestore-overview-v6"/"fhestore-overview.png"
    info = {
        "source":"Author-selected Figure 1 v6 and latest explicit production-only revision",
        "baseline":"../fhestore-overview-v6/fhestore-overview.png",
        "baseline_sha256":hashlib.sha256(ref.read_bytes()).hexdigest(),
        "canvas_units":list(CANVAS), "placement_width_mm":178,
        "placement_height_mm":178*CANVAS[1]/CANVAS[0],
        "generator":"build_drawio.py", "imagegen_used":False,
        "native_drawio":True,"baseline_style":"v6 light headers, rectangular components and original colors",
        "changes":["configure/execute and complete controls attach to Operator Pool",
                   "Ciphertext Relay renamed Ciphertext Transfer",
                   "remove pool-to-storage, Host-to-CSD and remote-to-Host blue return arrows",
                   "center remaining forward blue data path at y=270"],
        "palette":PALETTE,"topology":TOPOLOGY,
    }
    (ROOT/"source-info.json").write_text(json.dumps(info,indent=2)+"\n",encoding="utf-8")
    print("Wrote native drawio and source provenance:", ROOT)

if __name__ == "__main__":
    main()
