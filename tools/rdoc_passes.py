# Run inside RenderDoc's UI python: qrenderdoc --python tools/rdoc_passes.py
# Replays CAPTURE (env RDOC_CAPTURE) and writes, for every draw and dispatch, its output targets
# (format, size) and the colour at a few points of the first target, to RDOC_OUT.
import os
import sys
import traceback

import renderdoc as rd

capture = os.environ['RDOC_CAPTURE']
out_path = os.environ.get('RDOC_OUT', 'out/rdoc_passes.txt')
out = open(out_path, 'w')


def log(*a):
    out.write(' '.join(str(x) for x in a) + '\n')
    out.flush()


def main():
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile()
    result = cap.OpenFile(capture, '', None)
    if result != rd.ResultCode.Succeeded:
        log('open failed', result)
        return
    result, controller = cap.OpenCapture(rd.ReplayOptions(), None)
    if result != rd.ResultCode.Succeeded:
        log('replay failed', result)
        return
    textures = {t.resourceId: t for t in controller.GetTextures()}
    actions = []

    def walk(list_):
        for a in list_:
            if a.children:
                walk(a.children)
            if a.flags & (rd.ActionFlags.Drawcall | rd.ActionFlags.Dispatch | rd.ActionFlags.Clear):
                actions.append(a)
    walk(controller.GetRootActions())
    log('actions', len(actions))
    last_target = None
    for a in actions:
        targets = [o for o in a.outputs if o != rd.ResourceId.Null()]
        if not targets:
            log(a.eventId, a.GetName(controller.GetStructuredFile())[:60], 'no targets')
            continue
        controller.SetFrameEvent(a.eventId, True)
        t = textures.get(targets[0])
        desc = f'{t.width}x{t.height} {t.format.Name()}' if t else '?'
        vals = []
        if t and t.width > 1:
            for fx, fy in ((0.5, 0.5), (0.5, 0.8), (0.25, 0.3)):
                v = controller.PickPixel(targets[0], int(t.width * fx), int(t.height * fy),
                                         rd.Subresource(0, 0, 0), rd.CompType.Typeless)
                vals.append('(%.3f %.3f %.3f)' % tuple(v.floatValue[:3]))
        state = controller.GetPipelineState()
        ps = state.GetShaderReflection(rd.ShaderStage.Pixel)
        log(a.eventId, desc, 'targets', len(targets), ' '.join(vals),
            'ps', ps.resourceId if ps else '-', a.GetName(controller.GetStructuredFile())[:50])
    controller.Shutdown()
    cap.Shutdown()
    rd.ShutdownReplay()


try:
    main()
except Exception:
    log(traceback.format_exc())
out.close()
os._exit(0)
