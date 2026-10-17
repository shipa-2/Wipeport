# Run inside RenderDoc's UI python: qrenderdoc --python tools/rdoc_arrays.py
# The array textures of RDOC_CAPTURE that pixel shaders read: size, format, layers, how many draws
# read them; with RDOC_SAVE=DIR every layer of each (mip 0) as a PNG at the end of the frame.
import collections
import os
import traceback

import renderdoc as rd

out = open(os.environ.get('RDOC_OUT', 'out/rdoc_arrays.txt'), 'w')
try:
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile()
    cap.OpenFile(os.environ['RDOC_CAPTURE'], '', None)
    _, c = cap.OpenCapture(rd.ReplayOptions(), None)
    tex = {t.resourceId: t for t in c.GetTextures()}
    acts = []

    def walk(actions):
        for a in actions:
            if a.children:
                walk(a.children)
            if a.flags & rd.ActionFlags.Drawcall:
                acts.append(a.eventId)
    walk(c.GetRootActions())
    readers = collections.defaultdict(list)
    for e in acts:
        c.SetFrameEvent(e, False)
        for used in c.GetPipelineState().GetReadOnlyResources(rd.ShaderStage.Pixel):
            t = tex.get(used.descriptor.resource)
            if t and t.arraysize > 1:
                readers[t.resourceId].append(e)
    save = os.environ.get('RDOC_SAVE')
    if save:
        os.makedirs(save, exist_ok=True)
        c.SetFrameEvent(acts[-1], True)
    for rid, events in sorted(readers.items(), key=lambda kv: -len(kv[1])):
        t = tex[rid]
        out.write(f'{int(rid)} {t.width}x{t.height} {t.format.Name()} layers {t.arraysize} '
                  f'mips {t.mips} read by {len(events)} draws, first {events[0]} last {events[-1]}\n')
        if save:
            for layer in range(t.arraysize):
                s = rd.TextureSave()
                s.resourceId = rid
                s.mip = 0
                s.slice.sliceIndex = layer
                s.destType = rd.FileType.PNG
                s.alpha = rd.AlphaMapping.Discard
                c.SaveTexture(s, f'{save}/{int(rid)}_{layer}.png')
    c.Shutdown()
    cap.Shutdown()
except Exception:
    out.write(traceback.format_exc())
out.close()
os._exit(0)
