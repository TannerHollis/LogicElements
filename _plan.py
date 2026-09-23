import io, re, glob

# 1. Enumerate property reads (el.get("...").as_float) in compiler
s = io.open(r'src/compiler/src/le_compiler_core.cpp', encoding='utf-8').read()
props = set()
for m in re.finditer(r'el\.get\("([^"]+)"\)', s):
    props.add(m.group(1))
print('=== property keys read via el.get ===')
print(sorted(props))

# 2. Where does 'variables' / 'variable' appear?
for root in ('src', 'docs', 'tests', 'example_configs', 'examples'):
    for f in glob.glob(root + '/**/*', recursive=True):
        if not (f.endswith('.c') or f.endswith('.h') or f.endswith('.cpp') or f.endswith('.py') or f.endswith('.json') or f.endswith('.md') or f.endswith('.leconfig')):
            continue
        try:
            t = io.open(f, encoding='utf-8', errors='ignore').read()
        except Exception:
            continue
        for kw in ('variable', '%', 'pin_map', 'alias_to_addr', 'constants', '"constants"'):
            for m in re.finditer(re.escape(kw), t):
                ln = t.count('\n', 0, m.start()) + 1
                print(f'{f}:{ln}: {kw} | {t.splitlines()[ln-1].strip()[:80]}')
                break
            else:
                continue
            break