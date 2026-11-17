"""Every fixed text size in the native interface is a step of its type scale."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / 'native/src/App.h').read_text()
scale = re.search(r'caption = (\d+), body = (\d+), lead = (\d+), heading = (\d+), title = (\d+);', header)
assert scale, 'Type scale not found in App.h'
STEPS = {float(value) for value in scale.groups()}
# The size argument of each text helper, counted from zero.
SIZED = {'text': 2, 'textRight': 2, 'textBlock': 2, 'centred': 1, 'say': 1}
# Card faces, key caps and skin art size their text to the piece they are drawn on.
OUTSIDE = {'CardFace.cpp', 'Skin.cpp', 'Assets.cpp', 'Material.cpp', 'main.cpp', 'HostGL.cpp', 'HostDX11.cpp',
           'RendererDX11.cpp'}
NUMBER = re.compile(r'^\s*(\d+(?:\.\d+)?)f?\s*$')


def arguments(text, start):
    """The top-level arguments of the call whose '(' is at start."""
    depth, begin, quote, found, at = 0, start + 1, None, [], start
    while at < len(text):
        char = text[at]
        if quote:
            if char == '\\':
                at += 2
                continue
            if char == quote:
                quote = None
        elif char in '"\'':
            quote = char
        elif char in '([{':
            depth += 1
        elif char in ')]}':
            depth -= 1
            if depth == 0:
                found.append(text[begin:at])
                return found
        elif char == ',' and depth == 1:
            found.append(text[begin:at])
            begin = at + 1
        at += 1
    return []


checked, wrong = 0, []
for path in sorted((ROOT / 'native/src').glob('*.cpp')):
    if path.name in OUTSIDE:
        continue
    source = path.read_text()
    for call in re.finditer(r'(?<![A-Za-z0-9_:.>])(text|textRight|textBlock|centred|say)\(', source):
        found = arguments(source, call.end() - 1)
        if len(found) <= SIZED[call.group(1)]:
            continue
        literal = NUMBER.match(found[SIZED[call.group(1)]])
        if not literal:
            continue
        checked += 1
        if float(literal.group(1)) not in STEPS:
            wrong.append(f'{path.name}:{source.count(chr(10), 0, call.start()) + 1} {call.group(1)} at {literal.group(1)}')
    for measure in re.finditer(r'CalcTextSizeA\((\d+(?:\.\d+)?)f? \* scale', source):
        checked += 1
        if float(measure.group(1)) not in STEPS:
            wrong.append(f'{path.name}:{source.count(chr(10), 0, measure.start()) + 1} measures at {measure.group(1)}')
assert checked > 150, f'Only {checked} text sizes found; the helpers may have been renamed'
assert not wrong, 'Text sizes off the type scale:\n' + '\n'.join(wrong)
print(f'Type scale: {checked} fixed text sizes use the five steps {sorted(int(step) for step in STEPS)}')
