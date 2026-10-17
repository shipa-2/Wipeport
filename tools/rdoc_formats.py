import os, collections, traceback
import renderdoc as rd
out = open(os.environ['RDOC_OUT'], 'w')
try:
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile(); cap.OpenFile(os.environ['RDOC_CAPTURE'], '', None)
    _, c = cap.OpenCapture(rd.ReplayOptions(), None)
    cnt = collections.Counter(t.format.Name() for t in c.GetTextures())
    for k, v in cnt.most_common(): out.write(f'{v} {k}\n')
    c.Shutdown(); cap.Shutdown()
except Exception:
    out.write(traceback.format_exc())
out.close(); os._exit(0)
