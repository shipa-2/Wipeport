# Run inside RenderDoc's UI python: qrenderdoc --python tools/rdoc_history.py
# Pixel history of RDOC_X, RDOC_Y in the target written by RDOC_EVENT (its first colour output)
# of RDOC_CAPTURE: every event that touched the pixel, its values before and after, its shaders.
import os
import traceback

import renderdoc as rd

capture = os.environ['RDOC_CAPTURE']
event = int(os.environ['RDOC_EVENT'])
x, y = int(os.environ['RDOC_X']), int(os.environ['RDOC_Y'])
out = open(os.environ.get('RDOC_OUT', 'out/rdoc_history.txt'), 'w')


def log(*a):
    out.write(' '.join(str(v) for v in a) + '\n')
    out.flush()


def main():
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile()
    cap.OpenFile(capture, '', None)
    _, controller = cap.OpenCapture(rd.ReplayOptions(), None)
    controller.SetFrameEvent(event, True)
    state = controller.GetPipelineState()
    target = [o for o in state.GetOutputTargets() if o.resource != rd.ResourceId.Null()][0].resource
    log('target', target)
    tex = [t for t in controller.GetTextures() if t.resourceId == target][0]
    log("tex", tex.width, tex.height, tex.msSamp, tex.format.Name())
    history = controller.PixelHistory(target, x, y, rd.Subresource(0, 0, 0), rd.CompType.Float)
    log("history", len(history))
    sf = controller.GetStructuredFile()
    for m in history:
        if m.eventId > event:
            break
        controller.SetFrameEvent(m.eventId, True)
        st = controller.GetPipelineState()
        ps = st.GetShaderReflection(rd.ShaderStage.Pixel)
        action = controller.GetAction(m.eventId) if hasattr(controller, 'GetAction') else None
        pre = m.preMod.col.floatValue[:3]
        post = m.postMod.col.floatValue[:3]
        shad = m.shaderOut.col.floatValue[:4]
        log(m.eventId, 'pre (%.3f %.3f %.3f)' % tuple(pre), 'shader (%.3f %.3f %.3f %.3f)' % tuple(shad),
            'post (%.3f %.3f %.3f)' % tuple(post), 'passed' if m.Passed() else 'failed',
            'ps', ps.resourceId if ps else '-')
    controller.Shutdown()
    cap.Shutdown()


try:
    main()
except Exception:
    log(traceback.format_exc())
out.close()
os._exit(0)
