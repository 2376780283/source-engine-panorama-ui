#!/usr/bin/env python
# encoding: utf-8
# Auto-fetch loop for the panorama framework port:
#   build panorama target -> parse "cannot open file: X.h" -> locate X in
#   D:\\CSGO2019 (mirroring CSGO layout) -> copy to d:\\source-engine -> repeat.
# Stops when the build succeeds, or no more missing-file errors are fixable,
# or the iteration budget is exhausted. Semantic (non-missing-file) errors are
# reported at the end so they can be fixed by hand.
import subprocess, sys, os, re, shutil

SE   = r'd:\source-engine'
CSGO = r'D:\CSGO2019'
WAF  = os.path.join(SE, 'waf')
MAX_ITERS = 25
LOG  = os.path.join(SE, 'build', '_autofetch.log')

def log(msg):
    with open(LOG, 'a', encoding='utf-8') as f:
        f.write(msg + '\n')

# C1083 missing-file line. Chinese MSVC: 无法打开包括文件/源文件: "path"
MISSING_RE = re.compile(r"""fatal error C1083:\s*(?:无法打开|Cannot open)[^"'\r\n]*["']([^"']+?\.h)["']""", re.I)
MISSING_RE2 = re.compile(r"""fatal error C1083:\s*(?:无法打开|Cannot open)[^"'\r\n]*["']([^"']+?\.(?:h|hpp))["']""", re.I)

def run_build():
    env = dict(os.environ)
    wafbat = os.path.join(SE, 'waf.bat')
    # invoke via waf.bat (cmd) - same path that works interactively
    cmd = '"%s" build --targets=panorama' % wafbat
    p = subprocess.run(cmd, cwd=SE, shell=True, env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    data = p.stdout
    # try decode; messages are Chinese cp936, header paths are ASCII
    try:
        text = data.decode('gbk', errors='replace')
    except Exception:
        text = data.decode('utf-8', errors='replace')
    return p.returncode, text

def find_in_csgo(rel):
    rel = rel.replace('\\', '/')
    # candidate roots mirroring CSGO/SE include layout
    for base in ['public', 'common', '', 'engine', 'game/shared', 'panorama']:
        cand = os.path.join(CSGO, base, rel)
        if os.path.isfile(cand):
            return cand
    # last resort: search basename recursively (bounded)
    base = os.path.basename(rel)
    for root, _, files in os.walk(CSGO):
        if base in files:
            return os.path.join(root, base)
    return None

def main():
    # NOTE: do NOT delete d:\source-engine\.lock-waf_win32_build - waf uses the
    # top-level lock to locate the out dir in this repo.
    if os.path.exists(LOG):
        os.remove(LOG)
    log('autofetch start')
    for i in range(1, MAX_ITERS + 1):
        rc, out = run_build()
        if rc == 0:
            log('ITER %d: BUILD OK' % i)
            print('ITER %d: BUILD OK' % i)
            return 0
        missing = set()
        for m in MISSING_RE2.finditer(out):
            p = m.group(1).strip()
            if p and not p.lower().startswith(('e:\\', 'c:\\', 'd:\\source', 'windows kits', 'microsoft visual')):
                missing.add(p)
        if not missing:
            log('ITER %d: no fixable missing-file errors (semantic/build errors remain)' % i)
            # print last 40 lines of build output for diagnosis
            tail = '\n'.join(out.strip().splitlines()[-40:])
            log('--- tail ---\n' + tail)
            print('no fixable missing-file errors at iter %d; see %s' % (i, LOG))
            print(tail[-2000:])
            return 2
        copied = 0
        for rel in sorted(missing):
            src = find_in_csgo(rel)
            if src:
                dst = os.path.join(SE, os.path.relpath(src, CSGO))
                d = os.path.dirname(dst)
                if not os.path.isdir(d):
                    os.makedirs(d, exist_ok=True)
                shutil.copyfile(src, dst)
                log('ITER %d COPY %s -> %s' % (i, src, dst))
                print('copied %s -> %s' % (src, dst))
                copied += 1
            else:
                log('ITER %d NOT-FOUND-IN-CSGO: %s' % (i, rel))
                print('NOT-FOUND-IN-CSGO: %s' % rel)
        if copied == 0:
            print('nothing to copy at iter %d; stopping' % i)
            return 3
    print('iteration budget exhausted (%d)' % MAX_ITERS)
    return 4

if __name__ == '__main__':
    sys.exit(main())
