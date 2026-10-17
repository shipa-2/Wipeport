import os, collections, traceback
import renderdoc as rd
out = open(os.environ['RDOC_OUT'], 'w')
try:
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile(); cap.OpenFile(os.environ['RDOC_CAPTURE'], '', None)
    _, c = cap.OpenCapture(rd.ReplayOptions(), None)
    tex = {t.resourceId: t for t in c.GetTextures()}
    acts = []
    def walk(l):
        for a in l:
            if a.children: walk(a.children)
            if a.flags & rd.ActionFlags.Drawcall: acts.append(a.eventId)
    walk(c.GetRootActions())
    lo, hi = int(os.environ.get('LO', 0)), int(os.environ.get('HI', 10**9))
    pairs = collections.Counter()
    for e in acts:
        if not lo <= e <= hi: continue
        c.SetFrameEvent(e, False)
        st = c.GetPipelineState()
        for used in st.GetReadOnlyResources(rd.ShaderStage.Pixel):
            d = used.descriptor
            t = tex.get(d.resource)
            if not t: continue
            pairs[(t.format.Name(), d.format.Name())] += 1
    for k, v in pairs.most_common(): out.write(f'{v} image {k[0]} view {k[1]}\n')
    c.Shutdown(); cap.Shutdown()
except Exception:
    out.write(traceback.format_exc())
out.close(); os._exit(0)
