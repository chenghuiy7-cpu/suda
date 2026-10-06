#!/usr/bin/env python3
"""Synchronize the native figure with the author's revised noise-branch layout."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent / 'tfhe-encryption-v3' / 'tfhe-encryption.drawio'
SCALE, X_OFFSET, Y_OFFSET = .75, 14.5, 3


def main():
    tree = ET.parse(SOURCE)
    model = tree.getroot().find('./diagram/mxGraphModel')
    model.set('pageWidth', '1156')
    model.set('pageHeight', '502')
    model.set('dx', '1156')
    model.set('dy', '502')
    cells = {c.get('id'): c for c in model.findall('./root/mxCell')}
    for cell in cells.values():
        style = cell.get('style', '')
        style = re.sub(r'(fontSize|strokeWidth|endSize|jumpSize)=([0-9.]+)',
                       lambda m: f'{m[1]}={float(m[2])*SCALE:g}', style)
        if style:
            cell.set('style', style)
        geom = cell.find('mxGeometry')
        if geom is None:
            continue
        for name in ('width', 'height'):
            if geom.get(name) is not None:
                geom.set(name, str(float(geom.get(name)) * SCALE))
        if cell.get('vertex') == '1':
            for name, offset in (('x', X_OFFSET), ('y', Y_OFFSET)):
                if geom.get(name) is not None:
                    value = float(geom.get(name)) * SCALE
                    if cell.get('parent') == '1':
                        value += offset
                    geom.set(name, str(value))
        for point in geom.findall('.//mxPoint'):
            if point.get('x') is not None:
                point.set('x', str(float(point.get('x')) * SCALE + X_OFFSET))
            if point.get('y') is not None:
                point.set('y', str(float(point.get('y')) * SCALE + Y_OFFSET))

    # Match the author's shorter lower panels and raised arithmetic title.
    for id_ in ('cell-1', 'cell-4'):
        cells[id_].find('mxGeometry').set('height', '476')
    cells['cell-3'].find('mxGeometry').set('height', '296')
    cells['cell-57'].find('mxGeometry').set('y', '451.5')

    # Noise enters the final adder after a higher horizontal turn.
    points = cells['cell-41'].findall('./mxGeometry/Array/mxPoint')
    for point, (x, y) in zip(points, ((710.875, 156), (821, 156),
                                    (821, 340), (763.75, 340))):
        point.set('x', str(x))
        point.set('y', str(y))
    cells['cell-77'].find('mxGeometry').set('x', '795')
    cells['cell-77'].find('mxGeometry').set('y', '278')
    style = cells['cell-78'].get('style')
    cells['cell-78'].set('style', style.replace('fontFamily=Arial',
                            'fontFamily=Times New Roman').replace('fontStyle=0', 'fontStyle=2'))

    # The author's configuration branch has no filled junction marker.
    style = cells['junction-117-33'].get('style')
    cells['junction-117-33'].set('style', style.replace('fillColor=#202020',
                            'fillColor=none').replace('strokeColor=#202020', 'strokeColor=none'))

    # Width labels and graph connectivity are retained verbatim.
    assert cells['cell-72'].get('value') == 'w'
    assert cells['cell-98'].get('value') == 'W'
    assert cells['cell-41'].get('source') == 'noise-source'
    assert cells['cell-41'].get('target') == 'adder-333-192'
    ET.indent(tree, space='  ')
    target = HERE / 'tfhe-encryption.drawio'
    tree.write(target, encoding='utf-8', xml_declaration=True)
    shutil.copyfile(HERE.parent / 'tfhe-encryption-v3' / 'export_drawio.py',
                    HERE / 'export_drawio.py')
    info = {
        'source': str(SOURCE.relative_to(HERE.parent)),
        'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
        'author_reference': 'Revised Figure 2 supplied as an inline conversation image on 2026-10-05',
        'operation': 'Synchronize the existing native vector source with the visible author revision',
        'changes': ['Higher horizontal turn of noise branch before final adder',
                    'Relocated e_ell label', 'Shorter lower panels and raised arithmetic title',
                    'Configuration branch without filled junction dot'],
        'preserved': ['Input w and output W', 'All source/target connections',
                      'Sampler, selector, accumulator, encoding and packetization functions',
                      'Manuscript caption and Description'],
        'editable_source': 'tfhe-encryption.drawio',
        'builder': 'revise_drawio.py', 'exporter': 'export_drawio.py',
        'image_model_used': False,
        'placement': {'width_mm':178, 'height_mm':178*502/1156,
                      'source_width_px':1156, 'source_height_px':502},
    }
    (HERE / 'source-info.json').write_text(json.dumps(info, ensure_ascii=False, indent=2)+'\n')
    print(target)


if __name__ == '__main__':
    main()
