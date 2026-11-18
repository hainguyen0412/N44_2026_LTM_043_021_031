#!/usr/bin/env python3
"""
Sinh map 3D san truong DUT (v2) cho PX4 + Gazebo Harmonic. KHONG can Blender.

Chi can: python3 + pillow   (pip install pillow)

  python3 gen_campus_v2.py                       # sinh models/dut_campus/ (mesh .dae + texture + model.sdf)
  python3 gen_campus_v2.py --ground-map anh.png  # dung anh ban do cua ban lam nen dat thay cho nen tu ve
  python3 gen_campus_v2.py --preview             # xuat them preview_ground.png de xem nen dat

Sua phan CONFIG (BUILDINGS, ROADS, ...) roi chay lai.
"""
import argparse, math, os, random
from PIL import Image, ImageDraw, ImageFilter

# ===================== CONFIG =====================
S = 1.5                  # met / pixel cua anh map goc (257x300 px)
GATE_PX = (205, 288)     # cong chinh = goc toa do (0,0), drone spawn o day
IMG_W, IMG_H = 257, 300
FLIP_X = False           # map bi lat trai-phai -> True
FLIP_Y = False           # map bi lat tren-duoi -> True
FH = 3.6                 # chieu cao 1 tang (m)
PITCH = 4.0              # be rong 1 o cua so (m)
TREE_SEED = 7
N_TREES_RANDOM = 90

GREY = (0.78, 0.80, 0.84); RED = (0.72, 0.36, 0.33); BLUE = (0.55, 0.72, 0.85); WHITE = (0.92, 0.92, 0.92)
# (ten, x0, y0, x1, y1 theo pixel anh crop, cao_m, mau)
BUILDINGS = [
    ("khu_A_main",   132,  30, 200,  46, 18, GREY),
    ("khu_A_north",  128,   8, 185,  16, 12, GREY),
    ("toa_nha_thong_minh", 210, 8, 238, 40, 25, BLUE),
    ("khu_E_east",   235,  37, 246,  62, 12, RED),
    ("khu_G_1",       30,  20,  78,  32, 14, GREY),
    ("khu_G_2",       22,  42,  60,  56, 14, BLUE),
    ("khu_G_3",       55,  52, 110,  62, 12, GREY),
    ("khu_G_4",       98,  28, 128,  40, 12, GREY),
    ("khu_G_5",       80,  32,  95,  46, 10, RED),
    ("khu_G_6",       20,  22,  32,  38, 10, RED),
    ("khu_B_ngang",   32,  92, 118, 102, 14, RED),
    ("khu_B_doc",    118, 100, 128, 148, 14, RED),
    ("khu_B_trai",    32,  80,  40, 102, 12, RED),
    ("khu_F",        136, 100, 150, 150, 22, GREY),
    ("hoi_truong_F", 148, 128, 172, 140, 12, RED),
    ("khu_H",        156, 142, 185, 152, 10, GREY),
    ("khu_C",        213, 102, 244, 132, 16, GREY),
    ("thu_vien",     166, 198, 192, 222, 25, BLUE),
    ("nha_phai_1",   218, 185, 230, 205, 10, GREY),
    ("day_nha_phai_2", 210, 235, 218, 270, 10, WHITE),
    ("day_nha_phai_3", 237, 235, 245, 270, 10, WHITE),
    ("day_nha_duoi", 158, 265, 200, 280,  8, WHITE),
]
# Duong (polyline pixel, be rong px, co vach ke giua khong). DAY LA BO TRI UOC TINH - sua cho khop thuc te.
ROADS = [
    ([(205, 292), (205, 70)],  7, True),    # truc chinh tu cong len bac
    ([(20, 156), (252, 156)],  6, True),    # duong ngang giua
    ([(20, 70), (252, 70)],    5, False),   # duong ngang phia bac
    ([(130, 156), (130, 104)], 4, False),   # nhanh vao khu B/F
]
# ==================================================

HERE = os.path.dirname(os.path.abspath(__file__))
MODEL_DIR = os.path.join(HERE, "models", "dut_campus")
MESH_DIR = os.path.join(MODEL_DIR, "meshes")


def px2w(px, py):
    return ((px - GATE_PX[0]) * S, -(py - GATE_PX[1]) * S)


def rgb255(c, k=1.0):
    return tuple(max(0, min(255, int(v * 255 * k))) for v in c)


# ---------------------------------------------------------------- Mesh
class Mesh:
    def __init__(self):
        self.g = {}  # material -> dict(p, n, t)

    def tri(self, mat, a, b, c, ua, ub, uc):
        ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]
        vx, vy, vz = c[0]-a[0], c[1]-a[1], c[2]-a[2]
        nx, ny, nz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx
        l = math.sqrt(nx*nx+ny*ny+nz*nz) or 1.0
        n = (nx/l, ny/l, nz/l)
        d = self.g.setdefault(mat, {"p": [], "n": [], "t": []})
        for p, uv in ((a, ua), (b, ub), (c, uc)):
            d["p"].append(p); d["n"].append(n); d["t"].append(uv)

    def quad(self, mat, p0, p1, p2, p3, uvs=((0, 0), (1, 0), (1, 1), (0, 1))):
        self.tri(mat, p0, p1, p2, uvs[0], uvs[1], uvs[2])
        self.tri(mat, p0, p2, p3, uvs[0], uvs[2], uvs[3])

    def box(self, mat, x0, y0, z0, x1, y1, z1, top=None, bottom=False):
        """Hop dac, vat lieu don sac (top: vat lieu rieng cho mat tren)."""
        self.quad(mat, (x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1))
        self.quad(mat, (x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1))
        self.quad(mat, (x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1))
        self.quad(mat, (x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1))
        self.quad(top or mat, (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1))
        if bottom:
            self.quad(mat, (x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0))

    def facade_box(self, wall_mat, roof_mat, x0, y0, x1, y1, floors):
        h = floors * FH
        lx = max(1, round((x1 - x0) / PITCH))
        ly = max(1, round((y1 - y0) / PITCH))
        self.quad(wall_mat, (x0, y0, 0), (x1, y0, 0), (x1, y0, h), (x0, y0, h), ((0, 0), (lx, 0), (lx, floors), (0, floors)))
        self.quad(wall_mat, (x1, y1, 0), (x0, y1, 0), (x0, y1, h), (x1, y1, h), ((0, 0), (lx, 0), (lx, floors), (0, floors)))
        self.quad(wall_mat, (x1, y0, 0), (x1, y1, 0), (x1, y1, h), (x1, y0, h), ((0, 0), (ly, 0), (ly, floors), (0, floors)))
        self.quad(wall_mat, (x0, y1, 0), (x0, y0, 0), (x0, y0, h), (x0, y1, h), ((0, 0), (ly, 0), (ly, floors), (0, floors)))
        self.quad(roof_mat, (x0, y0, h), (x1, y0, h), (x1, y1, h), (x0, y1, h))
        return h

    def prism(self, mat, cx, cy, z0, z1, r, sides=6, top_mat=None):
        pts = [(cx + r*math.cos(2*math.pi*i/sides), cy + r*math.sin(2*math.pi*i/sides)) for i in range(sides)]
        for i in range(sides):
            a, b = pts[i], pts[(i+1) % sides]
            self.quad(mat, (a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1))
        for i in range(1, sides-1):
            self.tri(top_mat or mat, (pts[0][0], pts[0][1], z1), (pts[i][0], pts[i][1], z1), (pts[i+1][0], pts[i+1][1], z1),
                     (0, 0), (1, 0), (1, 1))

    def blob(self, mat, cx, cy, cz, r, rng, squash=0.85):
        """Tan cay low-poly: icosphere 1 lan chia + nhieu nhe cho tu nhien."""
        t = (1 + 5 ** 0.5) / 2
        v = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
             (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
        v = [tuple(c / math.sqrt(x*x+y*y+z*z) for c in (x, y, z)) for x, y, z in v]
        f = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
             (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
        cache = {}

        def mid(a, b):
            k = (min(a, b), max(a, b))
            if k not in cache:
                m = tuple((v[a][i] + v[b][i]) / 2 for i in range(3))
                l = math.sqrt(sum(c*c for c in m))
                v.append(tuple(c / l for c in m)); cache[k] = len(v) - 1
            return cache[k]
        nf = []
        for a, b, c in f:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            nf += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        jit = [1 + rng.uniform(-0.14, 0.14) for _ in v]
        P = [(cx + p[0]*r*j, cy + p[1]*r*j, cz + p[2]*r*j*squash) for p, j in zip(v, jit)]
        for a, b, c in nf:
            self.tri(mat, P[a], P[b], P[c], (0, 0), (1, 0), (1, 1))

    # --- Collada writer
    def write_dae(self, path, materials):
        """materials: ten -> ('color', (r,g,b)) | ('tex', 'file.png')"""
        imgs, fx, mats, geos, inst = [], [], [], [], []
        for name, (kind, val) in materials.items():
            if name not in self.g:
                continue
            if kind == "tex":
                imgs.append(f'<image id="img_{name}"><init_from>{val}</init_from></image>')
                diffuse = f'<texture texture="samp_{name}" texcoord="UVMap"/>'
                params = (f'<newparam sid="surf_{name}"><surface type="2D"><init_from>img_{name}</init_from></surface></newparam>'
                          f'<newparam sid="samp_{name}"><sampler2D><source>surf_{name}</source></sampler2D></newparam>')
                amb = "1 1 1 1"
            else:
                r, g, b = val
                diffuse = f"<color>{r:.3f} {g:.3f} {b:.3f} 1</color>"; params = ""; amb = f"{r:.3f} {g:.3f} {b:.3f} 1"
            fx.append(f'<effect id="fx_{name}"><profile_COMMON>{params}<technique sid="common"><phong>'
                      f'<ambient><color>{amb}</color></ambient><diffuse>{diffuse}</diffuse>'
                      f'<specular><color>0.05 0.05 0.05 1</color></specular><shininess><float>8</float></shininess>'
                      f'</phong></technique></profile_COMMON></effect>')
            mats.append(f'<material id="mat_{name}" name="{name}"><instance_effect url="#fx_{name}"/></material>')
            d = self.g[name]; n = len(d["p"])
            f3 = lambda L: " ".join(f"{c:.3f}" for p in L for c in p)
            geos.append(
                f'<geometry id="geo_{name}" name="{name}"><mesh>'
                f'<source id="geo_{name}-pos"><float_array id="geo_{name}-pos-a" count="{n*3}">{f3(d["p"])}</float_array>'
                f'<technique_common><accessor source="#geo_{name}-pos-a" count="{n}" stride="3"><param name="X" type="float"/><param name="Y" type="float"/><param name="Z" type="float"/></accessor></technique_common></source>'
                f'<source id="geo_{name}-nrm"><float_array id="geo_{name}-nrm-a" count="{n*3}">{f3(d["n"])}</float_array>'
                f'<technique_common><accessor source="#geo_{name}-nrm-a" count="{n}" stride="3"><param name="X" type="float"/><param name="Y" type="float"/><param name="Z" type="float"/></accessor></technique_common></source>'
                f'<source id="geo_{name}-uv"><float_array id="geo_{name}-uv-a" count="{n*2}">{f3(d["t"])}</float_array>'
                f'<technique_common><accessor source="#geo_{name}-uv-a" count="{n}" stride="2"><param name="S" type="float"/><param name="T" type="float"/></accessor></technique_common></source>'
                f'<vertices id="geo_{name}-vtx"><input semantic="POSITION" source="#geo_{name}-pos"/></vertices>'
                f'<triangles material="mat_{name}" count="{n//3}">'
                f'<input semantic="VERTEX" source="#geo_{name}-vtx" offset="0"/>'
                f'<input semantic="NORMAL" source="#geo_{name}-nrm" offset="1"/>'
                f'<input semantic="TEXCOORD" source="#geo_{name}-uv" offset="2" set="0"/>'
                f'<p>{" ".join(f"{i} {i} {i}" for i in range(n))}</p></triangles></mesh></geometry>')
            inst.append(f'<instance_geometry url="#geo_{name}"><bind_material><technique_common>'
                        f'<instance_material symbol="mat_{name}" target="#mat_{name}">'
                        f'<bind_vertex_input semantic="UVMap" input_semantic="TEXCOORD" input_set="0"/>'
                        f'</instance_material></technique_common></bind_material></instance_geometry>')
        xml = ('<?xml version="1.0" encoding="utf-8"?>\n'
               '<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">'
               '<asset><unit name="meter" meter="1"/><up_axis>Z_UP</up_axis></asset>'
               f'<library_images>{"".join(imgs)}</library_images>'
               f'<library_effects>{"".join(fx)}</library_effects>'
               f'<library_materials>{"".join(mats)}</library_materials>'
               f'<library_geometries>{"".join(geos)}</library_geometries>'
               f'<library_visual_scenes><visual_scene id="Scene"><node id="campus" name="campus">{"".join(inst)}</node></visual_scene></library_visual_scenes>'
               '<scene><instance_visual_scene url="#Scene"/></scene></COLLADA>')
        open(path, "w", encoding="utf-8").write(xml)


# ---------------------------------------------------------------- Textures
def noisy(im, amount=0.06):
    n = Image.effect_noise(im.size, 60).convert("RGB")
    return Image.blend(im, n, amount)


def facade_tile(color, glass=False, size=256):
    base = rgb255(color)
    im = Image.new("RGB", (size, size), base)
    im = noisy(im, 0.05)
    d = ImageDraw.Draw(im)
    dark = rgb255(color, 0.55)
    if glass:  # curtain wall: kinh gan kin, khung manh
        x0, y0, x1, y1 = int(size*.03), int(size*.08), int(size*.97), int(size*.86)
        for y in range(y0, y1):
            t = (y - y0) / (y1 - y0)
            col = (int(70 + 70*(1-t)), int(110 + 80*(1-t)), int(150 + 70*(1-t)))
            d.line([(x0, y), (x1, y)], fill=col)
        d.line([(size//2, y0), (size//2, y1)], fill=rgb255(WHITE, .85), width=5)
        d.rectangle([x0, y0, x1, y1], outline=rgb255(WHITE, .85), width=5)
        d.rectangle([0, int(size*.9), size, size], fill=rgb255(color, .8))
    else:
        x0, y0, x1, y1 = int(size*.2), int(size*.2), int(size*.8), int(size*.7)
        for y in range(y0, y1):
            t = (y - y0) / (y1 - y0)
            col = (int(60 + 80*(1-t)), int(95 + 90*(1-t)), int(125 + 95*(1-t)))
            d.line([(x0, y), (x1, y)], fill=col)
        d.line([(size//2, y0), (size//2, y1)], fill=(235, 235, 235), width=5)
        d.line([(x0, (y0+y1)//2), (x1, (y0+y1)//2)], fill=(235, 235, 235), width=3)
        d.rectangle([x0, y0, x1, y1], outline=(235, 235, 235), width=6)
        d.rectangle([x0-8, y1, x1+8, y1+9], fill=rgb255(color, 1.15))       # bau cua so
        d.rectangle([0, size-6, size, size], fill=dark)                       # dai san tang
        d.rectangle([0, 0, size, 4], fill=rgb255(color, 0.85))
    return im


def ground_texture(T=6):
    W, H = IMG_W * T, IMG_H * T
    rng = random.Random(3)
    im = Image.new("RGB", (W, H), (92, 128, 70))
    d = ImageDraw.Draw(im)
    for _ in range(260):  # mang co sang/toi
        x, y, r = rng.randint(0, W), rng.randint(0, H), rng.randint(30, 120)
        k = rng.uniform(0.9, 1.12)
        d.ellipse([x-r, y-r, x+r, y+r], fill=(int(92*k), int(128*k), int(70*k)))
    im = im.filter(ImageFilter.GaussianBlur(25))
    im = noisy(im, 0.10)
    d = ImageDraw.Draw(im)
    concrete, asphalt, curb = (176, 172, 164), (66, 68, 72), (205, 202, 195)
    # san be tong quanh nha
    for _, x0, y0, x1, y1, _, _ in BUILDINGS:
        m = 3
        d.rectangle([(min(x0, x1)-m)*T, (min(y0, y1)-m)*T, (max(x0, x1)+m)*T, (max(y0, y1)+m)*T], fill=concrete)
    # duong
    for pts, w, dashed in ROADS:
        P = [(x*T, y*T) for x, y in pts]
        d.line(P, fill=curb, width=int((w+2.2)*T), joint="curve")
    for pts, w, dashed in ROADS:
        P = [(x*T, y*T) for x, y in pts]
        d.line(P, fill=asphalt, width=int(w*T), joint="curve")
        if dashed:
            for (ax, ay), (bx, by) in zip(P, P[1:]):
                L = math.hypot(bx-ax, by-ay); ux, uy = (bx-ax)/L, (by-ay)/L
                s = 0
                while s < L:
                    e = min(s + 5*T, L)
                    d.line([(ax+ux*s, ay+uy*s), (ax+ux*e, ay+uy*e)], fill=(235, 220, 120), width=max(2, int(0.35*T)))
                    s += 10*T
    # bai san cong
    gx, gy = GATE_PX
    d.rectangle([(gx-9)*T, (gy-6)*T, (gx+9)*T, (gy+12)*T], fill=concrete)
    d.line([(gx*T, (gy+12)*T), (gx*T, (gy-6)*T)], fill=asphalt, width=int(7*T))
    im = noisy(im, 0.04)
    if FLIP_X: im = im.transpose(Image.FLIP_LEFT_RIGHT)
    if FLIP_Y: im = im.transpose(Image.FLIP_TOP_BOTTOM)
    return im


# ---------------------------------------------------------------- Layout helpers
def seg_dist(p, a, b):
    ax, ay = a; bx, by = b; px, py = p
    dx, dy = bx-ax, by-ay
    t = max(0, min(1, ((px-ax)*dx + (py-ay)*dy) / (dx*dx + dy*dy)))
    return math.hypot(px-(ax+t*dx), py-(ay+t*dy))


def near_road(p, extra=0.0):
    for pts, w, _ in ROADS:
        for a, b in zip(pts, pts[1:]):
            if seg_dist(p, a, b) < w/2 + 1.2 + extra:
                return True
    return False


def in_building(p, margin):
    for _, x0, y0, x1, y1, _, _ in BUILDINGS:
        if min(x0, x1)-margin <= p[0] <= max(x0, x1)+margin and min(y0, y1)-margin <= p[1] <= max(y0, y1)+margin:
            return True
    return False


# ---------------------------------------------------------------- Build
def build(ground_map=None, preview=False):
    os.makedirs(MESH_DIR, exist_ok=True)
    m = Mesh()
    rng = random.Random(TREE_SEED)
    materials = {}

    # --- textures
    for nm, col, glass in (("grey", GREY, False), ("red", RED, False), ("blue", BLUE, True), ("white", WHITE, False)):
        fn = f"facade_{nm}.png"
        facade_tile(col, glass).save(os.path.join(MESH_DIR, fn))
        materials[f"wall_{nm}"] = ("tex", fn)
    if ground_map:
        g = Image.open(ground_map).convert("RGB")
        g = g.resize((IMG_W*6, IMG_H*6), Image.LANCZOS)
        if FLIP_X: g = g.transpose(Image.FLIP_LEFT_RIGHT)
        if FLIP_Y: g = g.transpose(Image.FLIP_TOP_BOTTOM)
    else:
        g = ground_texture()
    g.save(os.path.join(MESH_DIR, "ground.png"))
    if preview:
        g.save(os.path.join(HERE, "preview_ground.png"))
    materials["ground"] = ("tex", "ground.png")
    materials["roof"] = ("color", (0.30, 0.31, 0.33))
    materials["trim"] = ("color", (0.86, 0.85, 0.82))
    materials["plinth"] = ("color", (0.55, 0.54, 0.52))
    materials["ac"] = ("color", (0.62, 0.64, 0.66))
    materials["trunk"] = ("color", (0.36, 0.25, 0.16))
    materials["leaf1"] = ("color", (0.22, 0.50, 0.20))
    materials["leaf2"] = ("color", (0.30, 0.58, 0.24))
    materials["leaf3"] = ("color", (0.17, 0.42, 0.19))
    materials["pole"] = ("color", (0.25, 0.26, 0.28))
    materials["lamp"] = ("color", (1.0, 0.95, 0.7))
    materials["gate_cap"] = ("color", (0.72, 0.18, 0.15))
    materials["flag"] = ("color", (0.85, 0.12, 0.10))
    materials["outer"] = ("color", (0.40, 0.50, 0.36))

    # --- nen dat
    x0, y0 = px2w(0, IMG_H); x1, y1 = px2w(IMG_W, 0)
    ua, ub = (1, 0) if FLIP_X and False else (0, 1)
    m.quad("ground", (x0, y0, 0), (x1, y0, 0), (x1, y1, 0), (x0, y1, 0), ((0, 0), (1, 0), (1, 1), (0, 1)))
    m.quad("outer", (-1500, -1500, -0.06), (1500, -1500, -0.06), (1500, 1500, -0.06), (-1500, 1500, -0.06))

    # --- nha
    col_name = {id(GREY): "grey", id(RED): "red", id(BLUE): "blue", id(WHITE): "white"}
    collisions = []
    for name, px0, py0, px1, py1, h, col in BUILDINGS:
        ax, ay = px2w(px0, py0); bx, by = px2w(px1, py1)
        wx0, wx1 = min(ax, bx), max(ax, bx); wy0, wy1 = min(ay, by), max(ay, by)
        floors = max(1, round(h / FH))
        mat = "wall_" + col_name[id(col)]
        top = m.facade_box(mat, "roof", wx0, wy0, wx1, wy1, floors)
        # chan tuong nho
        m.box("plinth", wx0-0.25, wy0-0.25, 0, wx1+0.25, wy1+0.25, 0.5)
        # lan can mai
        t, ph = 0.35, 0.9
        m.box("trim", wx0, wy0, top, wx1, wy0+t, top+ph)
        m.box("trim", wx0, wy1-t, top, wx1, wy1, top+ph)
        m.box("trim", wx0, wy0+t, top, wx0+t, wy1-t, top+ph)
        m.box("trim", wx1-t, wy0+t, top, wx1, wy1-t, top+ph)
        # may lanh tren mai
        if (wx1-wx0) * (wy1-wy0) > 400:
            cx, cy = (wx0+wx1)/2, (wy0+wy1)/2
            for dx in (-0.25, 0.25):
                px_, py_ = cx + dx*(wx1-wx0), cy
                m.box("ac", px_-1.5, py_-1.2, top, px_+1.5, py_+1.2, top+1.6)
        collisions.append((name, (wx0+wx1)/2, (wy0+wy1)/2, wx1-wx0, wy1-wy0, top))

    # --- cong truong + cot co
    gx, gy = 0.0, 0.0
    for sx in (-9.5, 9.5):
        m.box("plinth", gx+sx-0.8, gy-0.8, 0, gx+sx+0.8, gy+0.8, 4.2)
        m.box("gate_cap", gx+sx-1.0, gy-1.0, 4.2, gx+sx+1.0, gy+1.0, 4.8)
    fx, fy = gx + 14, gy + 8
    m.prism("pole", fx, fy, 0, 12, 0.12, 8)
    m.box("flag", fx, fy-0.02, 8.6, fx+3.0, fy+0.02, 11.6)

    # --- den duong doc truc chinh
    (mx, _), mw = ROADS[0][0][0], ROADS[0][1]
    for py in range(280, 75, -14):
        for side in (-1, 1):
            p = (mx + side * (mw/2 + 1.6), py)
            if in_building(p, 1):
                continue
            wx, wy = px2w(*p)
            m.prism("pole", wx, wy, 0, 5.5, 0.09, 6)
            m.box("lamp", wx-0.35, wy-0.35, 5.5, wx+0.35, wy+0.35, 5.8)

    # --- cay
    placed = []

    def try_tree(p, margin=3.5):
        if not (4 < p[0] < IMG_W-4 and 4 < p[1] < IMG_H-4):
            return
        if in_building(p, margin) or near_road(p, 0.8):
            return
        if math.hypot(p[0]-GATE_PX[0], p[1]-GATE_PX[1]) < 10:
            return
        for q in placed:
            if math.hypot(p[0]-q[0], p[1]-q[1]) < 3.2:
                return
        placed.append(p)
        wx, wy = px2w(*p)
        trunk_h = rng.uniform(2.4, 3.6); r = rng.uniform(1.9, 3.1)
        m.prism("trunk", wx, wy, 0, trunk_h+r*0.3, 0.22, 6)
        m.blob(rng.choice(("leaf1", "leaf2", "leaf3")), wx, wy, trunk_h + r*0.55, r, rng)

    # hang cay doc duong
    for pts, w, _ in ROADS:
        for a, b in zip(pts, pts[1:]):
            L = math.hypot(b[0]-a[0], b[1]-a[1])
            ux, uy = (b[0]-a[0])/L, (b[1]-a[1])/L
            nx, ny = -uy, ux
            s = 6
            while s < L:
                for side in (-1, 1):
                    off = w/2 + 3.2
                    try_tree((a[0]+ux*s+nx*off*side, a[1]+uy*s+ny*off*side))
                s += 6.5
    for _ in range(900):
        if len(placed) >= N_TREES_RANDOM + 40:
            break
        try_tree((rng.uniform(0, IMG_W), rng.uniform(0, IMG_H)))

    # --- ghi file
    m.write_dae(os.path.join(MESH_DIR, "campus.dae"), materials)

    coll = ""
    for n, cx, cy, sx, sy, hh in collisions:
        coll += (f'\n    <collision name="col_{n}"><pose>{cx:.2f} {cy:.2f} {hh/2:.2f} 0 0 0</pose>'
                 f'<geometry><box><size>{sx:.2f} {sy:.2f} {hh:.2f}</size></box></geometry></collision>')
    sdf = f"""<?xml version="1.0"?>
<sdf version="1.9">
  <model name="dut_campus">
    <static>true</static>
    <link name="campus">
      <visual name="campus_visual">
        <geometry><mesh><uri>model://dut_campus/meshes/campus.dae</uri></mesh></geometry>
      </visual>
      <collision name="ground_col"><pose>0 0 -0.05 0 0 0</pose>
        <geometry><box><size>3000 3000 0.1</size></box></geometry></collision>{coll}
    </link>
  </model>
</sdf>
"""
    open(os.path.join(MODEL_DIR, "model.sdf"), "w", encoding="utf-8").write(sdf)
    open(os.path.join(MODEL_DIR, "model.config"), "w", encoding="utf-8").write(
        '<?xml version="1.0"?>\n<model><name>dut_campus</name><version>2.0</version>'
        '<sdf version="1.9">model.sdf</sdf><description>DUT campus 3D (procedural)</description></model>\n')
    tris = sum(len(d["p"]) // 3 for d in m.g.values())
    print(f"OK: {len(BUILDINGS)} nha, {len(placed)} cay, {tris} tam giac -> {MODEL_DIR}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--ground-map", help="anh ban do lam nen dat (thay nen tu ve)")
    ap.add_argument("--preview", action="store_true")
    a = ap.parse_args()
    build(a.ground_map, a.preview)
