#!/usr/bin/env python3
"""Monochrome TFHE encryption IP, with editable SVG and vector PDF exports.

Visual reference: Howe, Practical Lattice-Based Cryptography in Hardware,
Fig. 4.1 (PDF p148 / printed p126). The connectivity remains FHEStore's.
"""
from pathlib import Path
from html import escape
import hashlib
import json
import subprocess

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[5]
INK = "#202020"
GRAY = "#bcbcbc"
WHITE = "#ffffff"
FONT = "Arial, Liberation Sans, sans-serif"
WIDTH, HEIGHT, WIDTH_MM = 504, 228, 178


class Figure:
    def __init__(self):
        self.elements = [
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH_MM}mm" '
            f'height="{WIDTH_MM * HEIGHT / WIDTH:.4f}mm" viewBox="0 6 {WIDTH} {HEIGHT}">',
            '<title>HLS architecture for TFHE encryption</title>',
            '<desc>Input and context, mask and noise sources, binary-key selection, '
            'dot accumulation with feedback, two body additions, and ciphertext packing.</desc>',
            '<defs><marker id="arrow" viewBox="0 0 8 8" refX="7.6" refY="4" '
            'markerWidth="5.2" markerHeight="5.2" orient="auto-start-reverse" '
            f'markerUnits="userSpaceOnUse"><path d="M0,0 L8,4 L0,8 Z" fill="{INK}"/>'
            '</marker></defs>',
        ]

    def rect(self, x, y, w, h, fill=WHITE, stroke_width=1):
        self.elements.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" '
                             f'fill="{fill}" stroke="{INK}" stroke-width="{stroke_width}"/>')

    def label(self, x, y, content, size=9, anchor="middle"):
        text = escape(content)
        text = text.replace('_ℓ', '<tspan baseline-shift="sub" font-size="6.8">ℓ</tspan>')
        self.elements.append(f'<text x="{x}" y="{y}" font-family="{FONT}" '
                             f'font-size="{size}" text-anchor="{anchor}" fill="{INK}">{text}</text>')

    def wire(self, points, arrow=True, sw=1):
        pts = " ".join(f"{x},{y}" for x, y in points)
        end = ' marker-end="url(#arrow)"' if arrow else ''
        self.elements.append(f'<polyline points="{pts}" fill="none" stroke="{INK}" '
                             f'stroke-width="{sw}" stroke-linejoin="miter"{end}/>')

    def dot(self, x, y):
        self.elements.append(f'<circle cx="{x}" cy="{y}" r="1.6" fill="{INK}"/>')

    def plus(self, x, y, r=10):
        self.elements.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{WHITE}" '
                             f'stroke="{INK}" stroke-width="1.1"/>')
        self.label(x, y+4.5, '+', 15)

    def mux(self, x, y, w, h):
        pts = f"{x},{y} {x+w},{y+h*.2} {x+w},{y+h*.8} {x},{y+h}"
        self.elements.append(f'<polygon points="{pts}" fill="{WHITE}" '
                             f'stroke="{INK}" stroke-width="1"/>')

    def bus(self, x, y, bits, direction="h"):
        if direction == "h":
            self.wire([(x-2, y+3), (x+2, y-3)], False, .9)
            self.label(x, y-6, bits, 8.5)
        else:
            self.wire([(x-3, y+2), (x+3, y-2)], False, .9)
            self.label(x+5, y+3, bits, 8.5, 'start')

    def save(self):
        svg = HERE / 'tfhe-encryption.svg'
        svg.write_text('\n'.join(self.elements + ['</svg>']) + '\n')
        subprocess.run(['inkscape', str(svg), '--export-type=pdf',
                        '--export-filename=' + str(HERE / 'tfhe-encryption.pdf')],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(['pdftoppm', '-r', '220', '-singlefile', '-png',
                        str(HERE / 'tfhe-encryption.pdf'), str(HERE / 'tfhe-encryption')],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def draw():
    g = Figure()
    # Functional regions follow the black/white hardware vocabulary of Fig.4.1.
    g.rect(20, 10, 105, 220, GRAY, 1.1)
    g.rect(129, 10, 245, 76, GRAY, 1.1)
    g.rect(129, 90, 245, 140, GRAY, 1.1)
    g.rect(378, 10, 106, 220, GRAY, 1.1)
    g.label(72.5, 26, 'INPUT / CONTEXT', 9)
    g.label(251.5, 26, 'SAMPLER', 10)
    g.label(251.5, 222, 'ARITHMETIC', 10)
    g.label(431, 26, 'CIPHERTEXTS', 10)

    # Context stores the packed key and sampler configuration, as in the code.
    g.rect(30, 40, 80, 28)
    g.label(70, 52, 'Secret key')
    g.label(70, 63, 's₀, …, sₙ₋₁', 8.8)
    g.rect(30, 78, 80, 27)
    g.label(70, 90, 'Config / seed')
    g.label(70, 101, 'n, Δ', 8.8)
    g.wire([(110, 91), (117, 91), (117, 33), (150, 33), (150, 42)])
    g.wire([(117, 33), (281, 33), (281, 42)])
    g.dot(117, 33)
    # Key-bit selection is a single gate, not coefficient multiplication.
    g.wire([(110, 54), (121, 54), (121, 97), (170, 97), (170, 112.8)])
    g.label(176, 106, 'sᵢ', 8.8)

    # Application input and on-IP radix encoding.
    g.rect(30, 133, 80, 58)
    g.label(70, 147, 'Radix encoder')
    g.label(70, 162, 'w → K × p', 8.8)
    g.label(70, 179, 'μ_ℓ = d_ℓΔ', 8.8)
    g.wire([(1, 153), (30, 153)])
    g.bus(14, 153, 'W')

    # Independent mask and noise sources, not replicated arithmetic lanes.
    g.rect(143, 42, 83, 28)
    g.label(184.5, 59, 'Mask PRNG')
    g.rect(270, 42, 79, 28)
    g.label(309.5, 59, 'Noise source')
    g.wire([(184.5, 70), (184.5, 102), (145, 102), (145, 123), (156, 123)])
    g.bus(184.5, 96, 'q', 'v')
    g.label(195, 75, 'aᵢ', 8.8, 'start')
    g.dot(184.5, 78)
    g.wire([(309.5, 70), (309.5, 74), (359, 74), (359, 179), (333, 179), (333, 182)])
    g.label(363, 167, 'e_ℓ', 8.8, 'start')
    # Mask emission crosses the independent noise wire without a junction.
    g.wire([(184.5, 78), (354, 78)], False)
    g.elements.append(f'<path d="M354,78 Q359,90 364,78" fill="none" '
                      f'stroke="{GRAY}" stroke-width="3.5"/>')
    g.elements.append(f'<path d="M354,78 Q359,90 364,78" fill="none" '
                      f'stroke="{INK}" stroke-width="1"/>')
    g.wire([(364, 78), (368, 78), (368, 137), (395, 137)])

    # Serial dot product, including the explicit accumulator feedback.
    g.mux(156, 110, 28, 34)
    g.label(143, 140, '0', 8.8)
    g.wire([(147, 136), (156, 136)])
    g.wire([(184, 127), (207, 127)])
    g.plus(217, 127)
    g.rect(253, 116, 52, 22)
    g.label(279, 131, 'dot')
    g.wire([(227, 127), (253, 127)])
    g.wire([(305, 127), (315, 127), (315, 103), (217, 103), (217, 117)])
    g.dot(315, 127)
    g.wire([(315, 127), (315, 173), (263, 173), (263, 182)])

    # Add encoded message and noise after the n mask coefficients.
    g.wire([(110, 172), (241, 172), (241, 192), (253, 192)])
    g.label(191, 166, 'μ_ℓ', 8.8)
    g.bus(228, 172, 'q')
    g.plus(263, 192)
    g.wire([(273, 192), (323, 192)])
    g.plus(333, 192)
    g.wire([(343, 192), (395, 192)])
    g.label(382, 185, 'b', 8.8)
    g.bus(363, 192, 'q')

    # Wide stream packing; word slots show packing, not arithmetic replication.
    g.rect(395, 95, 76, 112)
    g.label(433, 109, 'Packetizer')
    g.label(433, 125, 'q → W', 8.8)
    for i, word in enumerate(['q', 'q', '…', 'q']):
        g.rect(403+i*15, 148, 15, 17)
        g.label(410.5+i*15, 160, word, 8.8)
    g.label(433, 181, 'a₀, …, aₙ₋₁, b', 8.5)
    g.wire([(471, 156), (502, 156)])
    g.bus(491, 156, 'W')
    g.save()


def provenance():
    files = [REPO / 'device/operators/hls/lwe_encrypt/lwe_encrypt.cpp',
             REPO / 'device/operators/hls/lwe_encrypt/lwe_encrypt.hpp']
    info = {
        'renderer': 'Native editable SVG + Inkscape vector PDF + pdftoppm preview',
        'image_model_used': False,
        'visual_reference': {'paper': 'James Victor Howe, Practical Lattice-Based Cryptography in Hardware',
                             'path': '../../HLS figure paper/thesis.pdf',
                             'figure': '4.1', 'pdf_page': 148, 'printed_page': 126},
        'source_topology': 'FHEStore encryption datapath from ../tfhe-encryption-v1/',
        'repository_revision': subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', 'HEAD'], text=True).strip(),
        'source_hashes': {str(p.relative_to(REPO)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
        'placement': {'columns': 2, 'width_mm': WIDTH_MM, 'height_mm': WIDTH_MM*HEIGHT/WIDTH,
                      'minimum_font_pt': 8.5},
        'style': 'Gray functional regions, white rectangular modules and trapezoid MUX, circular adders, black orthogonal wires and bus-width marks.',
        'topology': ['context → encoder/samplers/key gate', 'W-bit input → radix encoder → μ',
                     'mask a_i → binary-key MUX(a_i,0) → dot accumulator',
                     'dot register → feedback addition', 'dot + μ + e → body b',
                     'mask and body → packetizer → W-bit output'],
        'parameter_labels': ['W', 'q', 'w', 'K', 'p', 'n', 'Δ'],
        'scope': 'Only Figure 2 is restyled; no system diagram, decryption datapath, or hardware code change.',
        'qa': {'semantic_evaluator': 'Independent agent review against CPU encrypt_encoded_lwe source and actual PNG',
               'visual_evaluator': 'Root agent inspection of the exported preview and final full-paper page 5',
               'checks': ['grayscale only', 'editable SVG without raster content',
                          'embedded PDF fonts', 'key MUX and feedback connectivity',
                          'body additions and mask bypass', 'readable labels at manuscript width',
                          'Figure 1 and all body prose unchanged', 'full-paper compilation with resolved references'],
               'formal_human_signoff': False},
    }
    (HERE / 'source-info.json').write_text(json.dumps(info, indent=2) + '\n')


if __name__ == '__main__':
    draw()
    provenance()
