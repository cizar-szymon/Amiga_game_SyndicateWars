#!/usr/bin/env python3
"""Run the ARM build of Syndicate Wars on a real Amiga and follow its log.

Uploads build/warp/swhost (to the game directory) and build/warp/swcore.wxt
(to RAM:), starts `swhost WXT=RAM:swcore.wxt <args>`, then reads
RAM:swarm-log.txt over one AmiAgent connection as often as it can, keeping
the longest copy in build/warp/swarm-log.txt - the Amiga may reset (an ARM
fault takes the board down), and then the last copy read is all there is.
After --secs seconds the game gets Ctrl-C (unless --keep).

usage: python tools/warp/run.py [--amiga2] [--secs N] [--keep] [game args...]
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import amiga_remote as ar  # noqa: E402

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
GDIR = "WorkSD:Games/SyndWars"


def connect(cfg):
    host, port, password = ar.load_settings(argparse.Namespace(config=cfg, host=None, port=None,
                                                               password=None))
    return ar.Agent(host, port, password, timeout=10)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--amiga2", action="store_true")
    ap.add_argument("--secs", type=float, default=40)
    ap.add_argument("--keep", action="store_true")
    # the options of this script first; everything after them goes to the game
    # as it is (argparse would reorder "-q -m 0,1")
    argv = sys.argv[1:]
    mine = []
    while argv and argv[0] in ("--amiga2", "--keep", "--secs"):
        mine.append(argv.pop(0))
        if mine[-1] == "--secs":
            mine.append(argv.pop(0))
    a = ap.parse_args(mine)
    a.args = argv
    cfg = "C:/amiga-dev/amiagent-2.cfg" if a.amiga2 else "C:/amiga-dev/amiagent.cfg"
    game_args = " ".join(a.args) if a.args else "-q -m 0,1"
    log_path = os.path.join(ROOT, "build", "warp", "swarm-log.txt")

    ag = connect(cfg)
    ag.put(open(os.path.join(ROOT, "build", "warp", "swhost"), "rb").read(), GDIR + "/swhost")
    ag.put(open(os.path.join(ROOT, "build", "warp", "swcore.wxt"), "rb").read(), "RAM:swcore.wxt")
    ag.put(("CD %s\nswhost WXT=RAM:swcore.wxt %s >NIL: *>NIL:\n" % (GDIR, game_args)).encode(),
           "RAM:swarm.script")
    ag.run("Delete RAM:swarm-log.txt QUIET")
    ag.start("Execute RAM:swarm.script")
    best = ""
    t0 = time.time()
    alive = True
    while time.time() - t0 < a.secs:
        try:
            rc, out = ag.run("Type RAM:swarm-log.txt")
        except Exception:
            alive = False
            break
        if rc == 0 and len(out) >= len(best):
            best = out
            open(log_path, "w", encoding="latin-1").write(best)
        if "swhost: stopped" in best or "extension state" in best:
            break
        time.sleep(0.1)
    if alive and not a.keep:
        try:
            ag.stop("swhost")
            time.sleep(4)
            rc, out = ag.run("Type RAM:swarm-log.txt")
            if rc == 0 and len(out) >= len(best):
                best = out
                open(log_path, "w", encoding="latin-1").write(best)
        except Exception:
            alive = False
    if not alive:
        best += "\n--- the Amiga stopped answering after %.1f s\n" % (time.time() - t0)
        open(log_path, "w", encoding="latin-1").write(best)
    sys.stdout.write("\n".join(best.splitlines()[-int(os.environ.get("LINES_OUT", "40")):]) + "\n")


if __name__ == "__main__":
    main()
