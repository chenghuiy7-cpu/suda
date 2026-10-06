#!/usr/bin/env python3
"""Apply the author's input-w/output-W revision to the native diagram."""
from pathlib import Path
import hashlib
import json
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent/'tfhe-encryption-v2'/'tfhe-encryption.drawio'


def main():
    tree = ET.parse(SOURCE)
    changed = []
    for cell in tree.getroot().findall('.//mxCell'):
        if cell.get('value') == 'W':
            geometry = cell.find('mxGeometry')
            if geometry is not None and float(geometry.get('x','0')) < 200:
                cell.set('value','w')
                changed.append(cell.get('id'))
    assert len(changed) == 1, changed
    ET.indent(tree,space='  ')
    target = HERE/'tfhe-encryption.drawio'
    tree.write(target,encoding='utf-8',xml_declaration=True)
    info = {
        'source': str(SOURCE.relative_to(HERE.parent)),
        'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
        'author_revision': 'Input w, output W; author-supplied edited diagram in conversation',
        'operation': 'Retain native component layout and connections; revise the input-width label',
        'input': 'w-bit application value supplied to the radix encoder by the input adapter',
        'output': 'W-bit ciphertext beat emitted by the packetizer',
        'renderer': 'Official diagrams.net viewer + mxCodec/mxImageExport; rsvg-convert exports',
        'editable_source': 'tfhe-encryption.drawio',
        'builder': 'revise_drawio.py',
        'exporter': 'export_drawio.py',
        'image_model_used': False,
        'placement': {'width_mm':178,'height_mm':178*684/1512},
    }
    (HERE/'source-info.json').write_text(json.dumps(info,indent=2)+'\n')
    print(target)


if __name__ == '__main__':
    main()
