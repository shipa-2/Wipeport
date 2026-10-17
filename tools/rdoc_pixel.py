# Run inside RenderDoc's UI python: qrenderdoc --python tools/rdoc_pixel.py
# Which draws of RDOC_CAPTURE wrote pixel RDOC_X, RDOC_Y of every colour target of RDOC_W x RDOC_H,
# and what their pixel shaders read (texture, size, format, layers, the view's mips and layers).
# RDOC_SAVE=DIR: the textures those draws read, mip 0 of their first viewed layer, as PNGs.
import os
import traceback

import renderdoc as rd

out = open(os.environ.get('RDOC_OUT', 'out/rdoc_pixel.txt'), 'w')


def log(*a):
    out.write(' '.join(str(v) for v in a) + '\n')
    out.flush()


def main():
    x, y = int(os.environ['RDOC_X']), int(os.environ['RDOC_Y'])
    w, h = int(os.environ.get('RDOC_W', 1920)), int(os.environ.get('RDOC_H', 1080))
    save = os.environ.get('RDOC_SAVE')
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile()
    cap.OpenFile(os.environ['RDOC_CAPTURE'], '', None)
    _, c = cap.OpenCapture(rd.ReplayOptions(), None)
    tex = {t.resourceId: t for t in c.GetTextures()}
    only = os.environ.get("RDOC_TARGET")
    targets = []
    for t in c.GetTextures():
        if t.width == w and t.height == h and t.arraysize == 1 and \
                (t.creationFlags & rd.TextureCategory.ColorTarget):
            targets.append(t.resourceId)
    saved = set()
    for target in [t for t in targets if not only or int(t) == int(only)]:
        history = c.PixelHistory(target, x, y, rd.Subresource(0, 0, 0), rd.CompType.Float)
        passed = [m for m in history if m.Passed()]
        if not passed:
            continue
        log('target', int(target), tex[target].format.Name(), 'draws', len(passed))
        for m in passed:
            c.SetFrameEvent(m.eventId, True)
            st = c.GetPipelineState()
            ps = st.GetShaderReflection(rd.ShaderStage.Pixel)
            log(' event', m.eventId, 'out (%.3f %.3f %.3f %.3f)' % tuple(m.shaderOut.col.floatValue[:4]),
                'ps', int(ps.resourceId) if ps else '-')
            for used in st.GetReadOnlyResources(rd.ShaderStage.Pixel):
                d = used.descriptor
                t = tex.get(d.resource)
                if not t:
                    continue
                log('   tex', int(t.resourceId), f'{t.width}x{t.height}', t.format.Name(),
                    'layers', t.arraysize, 'mips', t.mips, 'view mips', d.firstMip, d.numMips,
                    'layers', d.firstSlice, d.numSlices, 'type', str(d.textureType))
                if save and t.resourceId not in saved:
                    saved.add(t.resourceId)
                    s = rd.TextureSave()
                    s.resourceId = t.resourceId
                    s.mip = 0
                    s.slice.sliceIndex = d.firstSlice
                    s.destType = rd.FileType.PNG
                    s.alpha = rd.AlphaMapping.Discard
                    os.makedirs(save, exist_ok=True)
                    c.SaveTexture(s, f'{save}/{int(t.resourceId)}_{m.eventId}.png')
    c.Shutdown()
    cap.Shutdown()


try:
    main()
except Exception:
    log(traceback.format_exc())
out.close()
os._exit(0)
