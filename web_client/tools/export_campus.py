"""Export the supplied procedural campus to 2D assets. Python standard library only."""
import argparse
import ast
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]


def config(path):
    values={}
    def read(n):
        if isinstance(n,ast.Constant):return n.value
        if isinstance(n,ast.Name):return values[n.id]
        if isinstance(n,(ast.List,ast.Tuple)):return [read(v) for v in n.elts]
        if isinstance(n,ast.UnaryOp) and isinstance(n.op,ast.USub):return -read(n.operand)
        raise ValueError('Unsupported config expression; generator is not executed by exporter')
    names={'S','GATE_PX','IMG_W','IMG_H','FH','GREY','RED','BLUE','WHITE','BUILDINGS','ROADS','FLIP_X','FLIP_Y'}
    for node in ast.parse(path.read_text(encoding='utf-8')).body:
        if not isinstance(node,ast.Assign):continue
        for target in node.targets:
            if isinstance(target,ast.Name) and target.id in names:values[target.id]=read(node.value)
            elif isinstance(target,ast.Tuple) and all(isinstance(n,ast.Name) and n.id in names for n in target.elts):values.update(zip((n.id for n in target.elts),read(node.value)))
    if values['FLIP_X'] or values['FLIP_Y']:raise ValueError('Flipped texture requires explicit model/map calibration first')
    return values


def hull(points):
    p=sorted(set(points))
    if len(p)<3:return p
    def cross(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    def half(sequence):
        result=[]
        for point in sequence:
            while len(result)>=2 and cross(result[-2],result[-1],point)<=0:result.pop()
            result.append(point)
        return result
    return half(p)[:-1]+half(reversed(p))[:-1]


def export(source,output):
    generator=source/'gen_campus_v2.py';meshfile=source/'models/dut_campus/meshes/campus.dae';worldfile=source/'worlds/dut_campus.sdf'
    cfg=config(generator);gx,gy=cfg['GATE_PX'];scale=cfg['S']
    for required in ('models/dut_campus/model.sdf','models/dut_campus/model.config'):
        if not (source/required).is_file():raise ValueError('Missing campus input: '+required)
    mesh=ET.parse(meshfile).getroot();ns={'c':'http://www.collada.org/2005/11/COLLADASchema'}
    for image in mesh.findall('c:library_images/c:image/c:init_from',ns):
        texture=(meshfile.parent/image.text).resolve()
        if not texture.is_relative_to(source.resolve()) or not texture.is_file():raise ValueError('Missing or invalid texture: '+str(image.text))
    world=ET.parse(worldfile).getroot().find('world');origin=world.find('spherical_coordinates')
    if origin.findtext('world_frame_orientation','ENU')!='ENU' or float(origin.findtext('heading_deg','0'))!=0:raise ValueError('Exporter currently supports calibrated ENU heading 0 only')
    include=next(i for i in world.findall('include') if i.findtext('uri')=='model://dut_campus')
    if any(float(v)!=0 for v in include.findtext('pose','0 0 0 0 0 0').split()):raise ValueError('Campus has an offset/rotation: calibrate transform before export')
    buildings=[{'name':name,'x':x0,'y':y0,'w':x1-x0,'h':y1-y0,'roof_m':round(max(1,round(height/cfg['FH']))*cfg['FH'],2)} for name,x0,y0,x1,y1,height,color in cfg['BUILDINGS']]
    trees=[]
    for geo in mesh.findall('c:library_geometries/c:geometry',ns):
        if not geo.attrib['id'].startswith('geo_leaf'):continue
        vals=list(map(float,geo.find('c:mesh/c:source/c:float_array',ns).text.split()));vertices=list(zip(vals[::3],vals[1::3],vals[2::3]))
        if len(vertices)%240:raise ValueError('Unexpected leaf mesh layout')
        for offset in range(0,len(vertices),240):trees.append([[round(x,2),round(y,2)] for x,y in hull([(x/scale+gx,gy-y/scale) for x,y,z in vertices[offset:offset+240]])])
    names=[('Khu G',68,10),('Khu A',164,38),('TM',224,23),('Khu B',77,96),('Khu F',165,112),('H',170,147),('Khu C',228,117),('Thư viện',179,210)]
    data={'schema_version':1,'map_id':'dut_campus_v2','width':cfg['IMG_W'],'height':cfg['IMG_H'],'scale':scale,'gate':{'x':gx,'y':gy},'frame':'ENU',
          'origin':{'latitude':float(origin.findtext('latitude_deg')),'longitude':float(origin.findtext('longitude_deg')),'elevation':float(origin.findtext('elevation')),'heading_deg':0},
          'buildings':buildings,'roads':[{'points':pts,'width':w,'dashed':dashed} for pts,w,dashed in cfg['ROADS']],
          'trees':trees,'labels':[{'text':name,'x':x,'y':y} for name,x,y in names],
          'source_sha256':{str(p.relative_to(source)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in (generator,meshfile,worldfile)}}
    canonical=json.dumps(data,ensure_ascii=False,sort_keys=True,separators=(',',':')).encode('utf-8');data['version']=hashlib.sha256(canonical).hexdigest()
    output.mkdir(parents=True,exist_ok=True);(output/'campus_map.json').write_text(json.dumps(data,ensure_ascii=False,separators=(',',':'))+'\n',encoding='utf-8')
    svg=ET.Element('svg',xmlns='http://www.w3.org/2000/svg',viewBox=f"0 0 {cfg['IMG_W']} {cfg['IMG_H']}")
    ET.SubElement(svg,'rect',width=str(cfg['IMG_W']),height=str(cfg['IMG_H']),fill='#dce9d8')
    for b in buildings:ET.SubElement(svg,'rect',x=str(b['x']-3),y=str(b['y']-3),width=str(b['w']+6),height=str(b['h']+6),fill='#d8d9d1')
    for r in data['roads']:
        pts=' '.join(f'{x},{y}' for x,y in r['points']);ET.SubElement(svg,'polyline',points=pts,fill='none',stroke='#707a7d',**{'stroke-width':str(r['width'])})
        if r['dashed']:ET.SubElement(svg,'polyline',points=pts,fill='none',stroke='#f3e8ac',**{'stroke-width':'.35','stroke-dasharray':'5 5'})
    for p in trees:ET.SubElement(svg,'polygon',points=' '.join(f'{x},{y}' for x,y in p),fill='#b1ca9e')
    for b in buildings:ET.SubElement(svg,'rect',x=str(b['x']),y=str(b['y']),width=str(b['w']),height=str(b['h']),fill='#f0eee8',stroke='#949e9f',**{'stroke-width':'.4'})
    for name,x,y in names:ET.SubElement(svg,'text',x=str(x),y=str(y),fill='#273a2d',**{'font-size':'6','text-anchor':'middle','font-family':'sans-serif'}).text=name
    ET.ElementTree(svg).write(output/'campus_2d.svg',encoding='utf-8',xml_declaration=True)
    print(f"Exported {len(buildings)} buildings, {len(trees)} canopy projections; map version {data['version']}")


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,default=ROOT/'simulation/gazebo');p.add_argument('--output',type=Path,default=ROOT/'web_client/frontend/assets');args=p.parse_args();export(args.source.resolve(),args.output.resolve())
