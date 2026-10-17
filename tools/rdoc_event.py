# Run inside RenderDoc's UI python: qrenderdoc --python tools/rdoc_event.py
# For event RDOC_EVENT of RDOC_CAPTURE: the pixel shader's disassembly, its bound textures (size,
# format, a few values) and constant buffers (raw floats), into RDOC_OUT.
import os
import struct
import traceback

import renderdoc as rd

capture = os.environ['RDOC_CAPTURE']
event = int(os.environ['RDOC_EVENT'])
out = open(os.environ.get('RDOC_OUT', 'out/rdoc_event.txt'), 'w')


def log(*a):
    out.write(' '.join(str(x) for x in a) + '\n')
    out.flush()


def main():
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile()
    if cap.OpenFile(capture, '', None) != rd.ResultCode.Succeeded:
        log('open failed')
        return
    result, controller = cap.OpenCapture(rd.ReplayOptions(), None)
    controller.SetFrameEvent(event, True)
    textures = {t.resourceId: t for t in controller.GetTextures()}
    state = controller.GetPipelineState()
    for stage in (rd.ShaderStage.Vertex, rd.ShaderStage.Pixel):
        refl = state.GetShaderReflection(stage)
        if not refl:
            continue
        log('=== stage', stage, refl.resourceId)
        pipe = state.GetGraphicsPipelineObject()
        targets = controller.GetDisassemblyTargets(True)
        if stage == rd.ShaderStage.Pixel:
            log(controller.DisassembleShader(pipe, refl, targets[0]))
        # Textures
        for used in state.GetReadOnlyResources(stage):
            for desc in used.descriptors if hasattr(used, 'descriptors') else [used.descriptor]:
                rid = desc.resource
                t = textures.get(rid)
                if not t:
                    continue
                vals = []
                for fx, fy in ((0.5, 0.5), (0.0, 0.0), (0.5, 0.8)):
                    v = controller.PickPixel(rid, int((t.width - 1) * fx), int((t.height - 1) * fy),
                                             rd.Subresource(0, 0, 0), rd.CompType.Typeless)
                    vals.append('(%.4f %.4f %.4f %.4f)' % tuple(v.floatValue[:4]))
                log('texture', rid, f'{t.width}x{t.height}', t.format.Name(), ' '.join(vals))
        # Constant buffers: raw contents
        for i, cb in enumerate(state.GetConstantBlocks(stage)):
            d = cb.descriptor
            if d.resource == rd.ResourceId.Null():
                continue
            data = controller.GetBufferData(d.resource, d.byteOffset, min(d.byteSize or 256, 256))
            floats = struct.unpack('<%df' % (len(data) // 4), data[:len(data) // 4 * 4])
            log('cbuffer', i, d.resource, ' '.join('%g' % f for f in floats))
    controller.Shutdown()
    cap.Shutdown()


try:
    main()
except Exception:
    log(traceback.format_exc())
out.close()
os._exit(0)
