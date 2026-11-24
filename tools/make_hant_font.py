"""Cuts Noto Sans CJK TC down to the characters of the Traditional Chinese Forge menu (src/lang.inc).

MHGU Recomp's menu font has the Simplified Chinese characters but not the Traditional ones, so the plugin draws the
Traditional menu with this font (lang.inc's load_hant_font). Run it again whenever the Traditional texts change.

    py -m pip install fonttools
    py tools/make_hant_font.py NotoSansCJKtc-Regular.otf [fonts/cjk-hant.otf]

The source is github.com/notofonts/noto-cjk (Sans/OTF/TraditionalChinese), SIL Open Font License 1.1: the font goes
out with that license (OFL.txt beside it).
"""
import os
import re
import sys

from fontTools import subset

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHT = 5  # the column of g_texts (LANG_ENG, FRE, SPA, GER, ITA, CHT, CHS, KOR)
COLUMNS = 8


def columns(entry):
    """An entry's columns: the commas outside strings split them, each is its string literals joined (macros left out)"""
    cols, current, in_string, escaped = [], [], False, False
    for c in entry:
        if in_string:
            if escaped: escaped = False
            elif c == '\\': escaped = True
            elif c == '"': in_string = False
            else: current.append(c)
        elif c == '"': in_string = True
        elif c == ',': cols.append(''.join(current)); current = []
    cols.append(''.join(current))
    return cols


def menu_characters():
    text = open(os.path.join(ROOT, 'src', 'lang.inc'), encoding='utf-8').read()
    body = text[text.index('g_texts[T_COUNT][LANG_COUNT] = {'):text.index('// A text of the Forge menu')]
    chars = set()
    entries = re.findall(r'\[T_\w+\] = \{(.*?)\},', body, re.S)  # (an entry may end with a comment)
    count = len(re.findall(r'\[T_\w+\] = \{', body))
    if len(entries) != count:
        raise SystemExit(f'lang.inc: {len(entries)} of {count} entries read')
    for entry in entries:
        cols = columns(entry)
        if len(cols) < COLUMNS:
            raise SystemExit(f'lang.inc: an entry without {COLUMNS} columns: {entry[:60]}')
        chars |= set(cols[CHT])
    # The language list (drawn in this font while the menu is Traditional) and ImGui's own characters
    names = text[text.index('LANGUAGE_NAMES[LANG_COUNT] = {'):]
    chars |= set(names[:names.index('}')]) | set('繁體中文')
    chars |= {chr(c) for c in range(0x20, 0x7F)} | {'…', '�'}
    return sorted(c for c in chars if c >= ' ')


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    source = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'fonts', 'cjk-hant.otf')
    chars = menu_characters()
    options = subset.Options()
    options.layout_features = []  # (ImGui reads no layout tables)
    options.hinting = False
    options.notdef_outline = True
    options.name_IDs = ['*']
    options.name_languages = ['*']
    font = subset.load_font(source, options)
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=[ord(c) for c in chars])
    subsetter.subset(font)
    missing = [c for c in chars if ord(c) not in font.getBestCmap() and c not in '�']
    os.makedirs(os.path.dirname(out) or '.', exist_ok=True)
    subset.save_font(font, out, options)
    print(f'{out}: {len(chars)} characters, {os.path.getsize(out)} bytes')
    if missing:
        raise SystemExit('not in the source font: ' + ' '.join(missing))


if __name__ == '__main__':
    main()
