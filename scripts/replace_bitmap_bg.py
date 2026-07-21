import io
from pathlib import Path
p = Path(__file__).resolve().parents[1] / 'src' / 'weatherBitmaps.cpp'
s = p.read_text(encoding='utf-8')
new = s.replace('0x0000', '0xFFFF')
if new == s:
    print('No changes needed')
else:
    p.write_text(new, encoding='utf-8')
    print('Updated', p)
