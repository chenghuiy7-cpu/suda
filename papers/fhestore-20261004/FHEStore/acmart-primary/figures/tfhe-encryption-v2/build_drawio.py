#!/usr/bin/env python3
"""Convert the editable monochrome SVG to a native diagrams.net model."""
from pathlib import Path
from urllib.parse import quote
import base64
import math
import zlib
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
SCALE = 3
Y_ORIGIN = 6
INK = '#202020'


def stencil(points):
    x0 = min(x for x, y in points)
    y0 = min(y for x, y in points)
    width = max(x for x, y in points)-x0
    height = max(y for x, y in points)-y0
    shape = ET.Element('shape', {'name': 'Binary-key MUX', 'w': str(width),
                               'h': str(height), 'aspect': 'variable', 'strokewidth': 'inherit'})
    path = ET.SubElement(ET.SubElement(shape, 'background'), 'path')
    for i, (x, y) in enumerate(points):
        ET.SubElement(path, 'move' if i == 0 else 'line', {'x': str(x-x0), 'y': str(y-y0)})
    ET.SubElement(path, 'close')
    ET.SubElement(ET.SubElement(shape, 'foreground'), 'fillstroke')
    encoded = quote(ET.tostring(shape, encoding='unicode'), safe="~()*!.'-")
    compressor = zlib.compressobj(wbits=-15)
    return base64.b64encode(compressor.compress(encoded.encode()) + compressor.flush()).decode()


def main():
    svg = ET.parse(HERE/'tfhe-encryption.svg').getroot()
    doc = ET.Element('mxfile', {'host': 'app.diagrams.net', 'type': 'device', 'compressed': 'false'})
    diagram = ET.SubElement(doc, 'diagram', {'id': 'tfhe-encryption', 'name': 'TFHE Encryption'})
    model = ET.SubElement(diagram, 'mxGraphModel', {
        'dx': '1512', 'dy': '684', 'grid': '0', 'gridSize': '10',
        'guides': '1', 'tooltips': '1', 'connect': '1', 'arrows': '1',
        'fold': '1', 'page': '1', 'pageScale': '1', 'pageWidth': '1512',
        'pageHeight': '684', 'background': '#ffffff', 'math': '0', 'shadow': '0',
    })
    root = ET.SubElement(model, 'root')
    ET.SubElement(root, 'mxCell', {'id': '0'})
    ET.SubElement(root, 'mxCell', {'id': '1', 'parent': '0'})
    boxes, shape_cells, groups = {}, {}, {}
    serial = 0

    def cell(style, *, value='', parent='1', vertex=False, edge=False, id_=None, **attrs):
        nonlocal serial
        serial += 1
        attributes = {'id': id_ or f'cell-{serial}', 'value': value, 'style': style, 'parent': parent}
        if vertex:
            attributes['vertex'] = '1'
        if edge:
            attributes['edge'] = '1'
        attributes.update(attrs)
        return ET.SubElement(root, 'mxCell', attributes)

    def geometry(c, x, y, width, height, parent='1'):
        if parent in boxes:
            x -= boxes[parent][0]
            y -= boxes[parent][1]
        else:
            y -= Y_ORIGIN
        ET.SubElement(c, 'mxGeometry', {'x': str(x*SCALE), 'y': str(y*SCALE),
                                     'width': str(width*SCALE), 'height': str(height*SCALE),
                                     'as': 'geometry'})

    def group(id_, box):
        c = cell('group;container=1;collapsible=0;recursiveResize=0;html=0;', vertex=True, id_=id_)
        geometry(c, *box)
        boxes[id_] = box
        groups[id_] = box
        return id_

    names = {(30., 40.): 'secret-key', (30., 78.): 'configuration', (30., 133.): 'radix-encoder',
             (143., 42.): 'mask-generator', (270., 42.): 'noise-source',
             (253., 116.): 'dot-register', (395., 95.): 'packetizer'}
    # Keep native module boundaries and labels together for ordinary editing.
    elements = list(svg)
    for e in elements:
        tag = e.tag.rsplit('}', 1)[-1]
        if tag == 'rect':
            box = tuple(float(e.attrib[a]) for a in ('x', 'y', 'width', 'height'))
            x, y, w, h = box
            name = names.get((x, y))
            if name:
                parent = group(name+'-group', box)
            elif y == 148 and x >= 403:
                parent = 'packetizer-group'
            else:
                parent = '1'
            style = f'rounded=0;html=0;fillColor={e.attrib["fill"]};strokeColor={e.attrib["stroke"]};strokeWidth={float(e.attrib["stroke-width"])*SCALE};shadow=0;'
            c = cell(style, vertex=True, parent=parent, id_=name)
            geometry(c, *box, parent)
            shape_cells[c.get('id')] = ('rect', box)
        elif tag == 'circle':
            cx, cy, r = (float(e.attrib[a]) for a in ('cx', 'cy', 'r'))
            box = (cx-r, cy-r, 2*r, 2*r)
            if r > 5:
                name = f'adder-{int(cx)}-{int(cy)}'
                parent = group(name+'-group', box)
            else:
                name = f'junction-{cx:g}-{cy:g}'
                parent = '1'
            style = f'ellipse;html=0;fillColor={e.attrib["fill"]};strokeColor={e.attrib.get("stroke",e.attrib["fill"])};strokeWidth={float(e.attrib.get("stroke-width",0))*SCALE};'
            c = cell(style, vertex=True, parent=parent, id_=name)
            geometry(c, *box, parent)
            shape_cells[name] = ('circle', box)
        elif tag == 'polygon':
            points = [tuple(map(float, pair.split(','))) for pair in e.attrib['points'].split()]
            x0, y0 = min(x for x,y in points), min(y for x,y in points)
            box = (x0,y0,max(x for x,y in points)-x0,max(y for x,y in points)-y0)
            style = f'shape=stencil({stencil(points)});html=0;fillColor=#ffffff;strokeColor={INK};strokeWidth={SCALE};'
            parent = group('binary-key-mux-group', box)
            c = cell(style, vertex=True, parent=parent, id_='binary-key-mux')
            geometry(c, *box, parent)
            shape_cells['binary-key-mux'] = ('mux', box)

    def attach(point):
        x, y = point
        candidates = []
        for id_, (kind, box) in shape_cells.items():
            bx, by, w, h = box
            if kind == 'circle':
                distance = abs(math.hypot(x-bx-w/2, y-by-h/2)-w/2)
                if w < 5:
                    distance = math.hypot(x-bx-w/2, y-by-h/2)
            elif kind == 'mux':
                points = [(bx,by),(bx+w,by+h*.2),(bx+w,by+h*.8),(bx,by+h)]
                distance = float('inf')
                for (ax,ay),(cx,cy) in zip(points,points[1:]+points[:1]):
                    t = max(0,min(1,((x-ax)*(cx-ax)+(y-ay)*(cy-ay))/((cx-ax)**2+(cy-ay)**2)))
                    distance = min(distance,math.hypot(x-ax-t*(cx-ax),y-ay-t*(cy-ay)))
            else:
                if not (bx-.5 <= x <= bx+w+.5 and by-.5 <= y <= by+h+.5):
                    continue
                distance = min(abs(x-bx), abs(x-bx-w), abs(y-by), abs(y-by-h))
            if distance <= .6:
                candidates.append((w*h, id_, box))
        if candidates:
            _, id_, box = min(candidates)
            return id_, box
        return None

    for e in elements:
        if e.tag.rsplit('}',1)[-1] != 'polyline':
            continue
        points = [tuple(map(float, pair.split(','))) for pair in e.attrib['points'].split()]
        if points[0] == (364.,78.):
            continue
        mask_branch = points == [(184.5,78.),(354.,78.)]
        if mask_branch:
            points = [(184.5,78.),(368.,78.),(368.,137.),(395.,137.)]
        arrow = 'marker-end' in e.attrib or mask_branch
        style = (f'edgeStyle=none;rounded=0;html=0;strokeColor={e.attrib["stroke"]};'
                 f'strokeWidth={float(e.attrib["stroke-width"])*SCALE};'
                 f'endArrow={"block" if arrow else "none"};endFill=1;endSize={5.2*SCALE};startArrow=none;')
        if mask_branch:
            style += f'jumpStyle=arc;jumpSize={10*SCALE};'
        attrs = {}
        for end, point, prefix in [('source',points[0],'exit'),('target',points[-1],'entry')]:
            terminal = attach(point)
            if terminal:
                id_, (x,y,w,h) = terminal
                attrs[end] = id_
                style += f'{prefix}X={(point[0]-x)/w};{prefix}Y={(point[1]-y)/h};{prefix}Perimeter=0;'
        c = cell(style, edge=True, id_='mask-output' if mask_branch else None, **attrs)
        geo = ET.SubElement(c,'mxGeometry',{'relative':'1','as':'geometry'})
        for end, point in [('sourcePoint',points[0]),('targetPoint',points[-1])]:
            ET.SubElement(geo,'mxPoint',{'x':str(point[0]*SCALE),'y':str((point[1]-Y_ORIGIN)*SCALE),'as':end})
        if len(points)>2:
            arr=ET.SubElement(geo,'Array',{'as':'points'})
            for x,y in points[1:-1]:
                ET.SubElement(arr,'mxPoint',{'x':str(x*SCALE),'y':str((y-Y_ORIGIN)*SCALE)})

    for e in elements:
        if e.tag.rsplit('}',1)[-1] != 'text':
            continue
        x, y, size = float(e.attrib['x']), float(e.attrib['y']), float(e.attrib['font-size'])
        plain = ''.join(e.itertext())
        parent = '1'
        owners = [(w*h,id_) for id_,(bx,by,w,h) in groups.items() if bx <= x <= bx+w and by <= y <= by+h]
        if owners:
            parent = min(owners)[1]
        width = max(8,len(plain)*size*.64+6)
        anchor = e.attrib.get('text-anchor','middle')
        left = x-(width/2 if anchor=='middle' else width if anchor=='end' else 0)
        # Native text runs preserve subscripts in both the editor and SVG export.
        if len(e):
            runs = [(e.text or '',size,0)]
            for span in e:
                runs.append((span.text or '',float(span.get('font-size',size*.76)),size*.25))
                runs.append((span.tail or '',size,0))
            runs = [run for run in runs if run[0]]
            def advance(text,font_size):
                return sum({' ': .278, '=': .584, 'Δ': .668, 'ℓ': .415}.get(ch,.556)*font_size for ch in text)
            total = sum(advance(text,font_size) for text,font_size,_ in runs)
            left = x-(total/2 if anchor=='middle' else total if anchor=='end' else 0)
            g = cell('group;container=1;collapsible=0;recursiveResize=0;html=0;',vertex=True,parent=parent)
            geometry(g,left,y-size,total,size*1.4,parent)
            formula_id = g.get('id')
            boxes[formula_id] = (left,y-size,total,size*1.4)
            offset = 0
            for text,font_size,baseline_shift in runs:
                run_width = advance(text,font_size)
                style = (f'text;html=0;strokeColor=none;fillColor=none;align=left;verticalAlign=middle;'
                         f'whiteSpace=nowrap;overflow=visible;fontFamily=Arial;fontSize={font_size*SCALE};'
                         f'fontColor={INK};fontStyle=0;spacing=0;')
                c = cell(style,value=text,vertex=True,parent=formula_id)
                geometry(c,left+offset,y+baseline_shift-font_size,run_width+2,font_size*1.2,formula_id)
                offset += run_width
            continue
        style = (f'text;html=0;strokeColor=none;fillColor=none;align={"center" if anchor=="middle" else "right" if anchor=="end" else "left"};'
                 f'verticalAlign=middle;whiteSpace=nowrap;overflow=visible;fontFamily=Arial;fontSize={size*SCALE};'
                 f'fontColor={INK};fontStyle=0;spacing=0;')
        c = cell(style,value=plain,vertex=True,parent=parent)
        geometry(c,left,y-size,width,size*1.2,parent)

    ET.indent(doc,space='  ')
    path = HERE/'tfhe-encryption.drawio'
    ET.ElementTree(doc).write(path,encoding='utf-8',xml_declaration=True)
    cells = list(root)
    print(f'{path}: {sum(c.get("vertex")=="1" for c in cells)} native vertices, '
          f'{sum(c.get("edge")=="1" for c in cells)} editable connectors, {len(groups)} component groups')


if __name__ == '__main__':
    main()
