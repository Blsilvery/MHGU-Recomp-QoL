"""Builds the mod's reward screen archives from your own game files (the repository holds no game data).

usage: python3 -I tools/make_arcs.py [recomp folder] [mod folder]

Run from the installed mod (mods/field-qol/tools/make_arcs.py), it needs no arguments: the mod folder is the one
holding tools/, and the recomp folder the one holding mods/. The game files are read from <recomp folder>/data.

The reward screen's menu (eng\\GUI\\01_quest\\result\\reward) gets a fourth row under "Send to the Item Box" for the
plugin's "Send all to Item Box & Exit": a copy of the third row one row lower, a second separator line one row lower, a
taller window and the "Time Left" box under it one row lower. The appraisal's menu, two rows, gets its third row shown,
a taller window and the box under it one row lower. The plugin gives the rows their texts and actions. The archive
holding the layout is copied whole with that one file changed: a mod's archive replaces the game's (the recomp merges two
mods' archives, not a mod's and the game's).
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from patchgui import Patch

LAYOUT = 'GUI\\01_quest\\result\\reward'
TRADE_LAYOUT = 'GUI\\04_lobby\\shop\\shop_trade'
GUI_TYPE = 0x22948394
# the archives, each with the layout it holds and how it's patched (patch_reward, patch_trade, below)
ARCS = [('arc/result_%s.arc', LAYOUT), ('arc/facility/shoptrade_%s.arc', TRADE_LAYOUT),
        ('arc/village/common_%s.arc', LAYOUT)]
LANGS = ['eng', 'fre', 'spa', 'ger', 'ita', 'chT', 'chS']
GMD_TYPE = 0x242bb29a
# Native village message bank indices. Group zero belongs to the resident bank.
VILLAGE_MESSAGES = (
    None, 'QuestCounter', 'LobbyMsg', 'LobbyMenuMsg', 'LbHelp', 'OtBoardMsg',
    'GuildCardMsg', 'GC_Title_1', 'GC_Title_2', 'GC_Medal', 'GC_background',
    'GuestHunterMsg', 'guestQuestData', 'EqShop', 'LbShop_EqMakeMsg', 'ItemShop',
    'LbShop_Eq_EqMakeMsg', 'LbShop_OtEqMsg', 'LbShop_OtEmployMsg', 'Kitchen',
    'Lb_AirshipMsg', 'Lb_PostMsg', 'Lb_ItemBoxMsg', 'Lb_ArenaCounterMsg',
    'Lb_MonnyanMsg', 'Lb_OtDoujoMsg', 'Lb_TradeShopMsg', 'Room_s', 'Lb_GalleryMsg',
    'Lb_PetMsg', 'Lb_MakaRenkinMsg', 'Lb_HeroBlade', 'Lb_CaveMsg', 'Lb_AshiyuMsg',
    'Lb_RyurekiinMsg', 'Lb_KarisutaMsg', 'CharMakeMsg', 'Lb_LaboratoryMsg',
    'researchReinforce', 'NetworkVillage', 'KeyboardMsg', 'ResultMsg',
)

ROW_STEP = 34.0      # the rows are at 39, 73, 108
NEW_ROW_Y = 142.0
# The menu's states (flow params) size it for two or three rows: the window's SclY 5.8 / 7.5, the "Time Left" box
# under it at 188 / 222. Three rows become four:
WINDOW_SCALE_Y = (7.5, 9.2)
TIME_LEFT_Y = (222.0, 256.0)
# The appraisal (its own state, reward2a: the fourth state in the order of the flows' IDs) has two rows, Send to Boxes and
# Sell Items & Exit, which become three: a taller window (SclY 5.9), its third row shown, and the box under it (Time
# Left and the box slots, which only that state shows) one row lower
APPRAISAL_STATE = 3
WINDOW_SCALE_Y_APPRAISAL = (5.9, 7.6)
APPRAISAL_BOX = 'Nul_sub3'
APPRAISAL_BOX_Y = (188.0, 222.0)

# The trade cart's Claim Items (Neko): Take Items, Send to Item Box, Sell Items & Exit at 37.5 / 71 / 106, a window of SclY
# 7.6 (its flow: 7.45) and Cart Info (Nul_main0) under it at 275. A fourth row, one row lower each.
TRADE_NEW_ROW_Y = 140.0
TRADE_WINDOW_SCALE_Y = (7.45, 9.15)
TRADE_WINDOW_SCALE_Y_INIT = (7.6, 9.3)
TRADE_CART_INFO_Y = (275.0, 309.0)


def set_init_float(p, inst_name, name, old, new):
    i = p.by_name('inst', inst_name)
    v = [x for n, _, x in p.g.inits(p.g.rec['inst'][i]) if n == name]
    assert len(v) == 1 and abs(v[0] - old) < 1e-4, (inst_name, name, v)
    p.set_float('inst', i, name, new)


def patch_trade(data):
    p = Patch(data)
    p.clone_instance('Anm_mesS02', 'Anm_mesS03', 'Anm_mesS02', {'PosY': TRADE_NEW_ROW_Y})
    p.clone_instance('Anm_line_sub', 'Anm_line_sub_fq', 'Anm_line_sub', {'PosY': ROW_STEP})
    p.set_param_floats('Anm_Window_sub', 'SclY', *TRADE_WINDOW_SCALE_Y)
    set_init_float(p, 'Anm_Window_sub', 'SclY', *TRADE_WINDOW_SCALE_Y_INIT)
    set_init_float(p, 'Nul_main0', 'PosY', *TRADE_CART_INFO_Y)
    return p.build()


def patch_layout(data):
    p = Patch(data)
    p.clone_instance('Anm_mesS02', 'Anm_mesS03', 'Anm_mesS02', {'PosY': NEW_ROW_Y})
    p.clone_instance('Anm_line_sub', 'Anm_line_sub_fq', 'Anm_line_sub', {'PosY': ROW_STEP})
    p.set_param_floats('Anm_Window_sub', 'SclY', *WINDOW_SCALE_Y)
    p.set_param_floats('Nul_sub1', 'PosY', *TIME_LEFT_Y)
    p.set_param_floats('Anm_Window_sub', 'SclY', *WINDOW_SCALE_Y_APPRAISAL)
    p.set_exe_flag('Anm_mesS02', APPRAISAL_STATE, True)
    box = p.by_name('inst', APPRAISAL_BOX)
    y = [v for n, _, v in p.g.inits(p.g.rec['inst'][box]) if n == 'PosY']
    assert len(y) == 1 and abs(y[0] - APPRAISAL_BOX_Y[0]) < 1e-4, (APPRAISAL_BOX, y)
    p.set_float('inst', box, 'PosY', APPRAISAL_BOX_Y[1])
    return p.build()


def patch_arc(arc, layout):
    """The archive with its layout (LAYOUT or TRADE_LAYOUT) patched, or None if it has none."""
    version, count = struct.unpack_from('<HH', arc, 4)
    entries = []
    for i in range(count):
        o = 12 + i * 0x50
        name = arc[o:o + 0x40].split(b'\0')[0].decode('latin1')
        typ, csize, sf, off = struct.unpack_from('<IIII', arc, o + 0x40)
        entries.append([name, typ, sf, arc[off:off + csize]])
    hits = [e for e in entries if e[1] == GUI_TYPE and e[0].endswith(layout)]
    if not hits:
        return None
    for e in hits:
        data = (patch_trade if layout == TRADE_LAYOUT else patch_layout)(zlib.decompress(e[3]))
        e[3] = zlib.compress(data, 9)
        e[2] = (e[2] & ~0x1FFFFFFF) | len(data)
    head = 12 + count * 0x50
    out = bytearray(struct.pack('<4sHHI', b'ARC\0', version, count, 0))
    data_at = (head + 0x7FFF) // 0x8000 * 0x8000
    blobs = bytearray()
    for name, typ, sf, blob in entries:
        out += name.encode('latin1').ljust(0x40, b'\0') + struct.pack('<IIII', typ, len(blob), sf, data_at + len(blobs))
        blobs += blob
    out += b'\0' * (data_at - len(out))
    return bytes(out + blobs)


def camp_message_files(village, lang):
    tables = {}
    for i in range(struct.unpack_from('<H', village, 6)[0]):
        offset = 12 + i * 0x50
        kind, size, flags, start = struct.unpack_from('<4I', village, offset + 0x40)
        if kind != GMD_TYPE:
            continue
        basename = village[offset:offset + 0x40].split(b'\0', 1)[0].decode('latin1').rsplit('\\', 1)[-1]
        tables[basename] = (size, flags, start)
    for index, table in enumerate(VILLAGE_MESSAGES[1:], 1):
        size, flags, start = tables[f'{table}_{lang}']
        message = zlib.decompress(village[start:start + size])
        if not message.startswith(b'GMD\0') or len(message) != flags & 0x1fffffff:
            raise ValueError(f'Invalid village message: {table}_{lang}')
        yield f'romfs/nativeNX/wheelpl/qolcamp/{lang}_{index:02d}.gmd', message


def main(recomp, mod):
    native = os.path.join(recomp, 'data', 'romfs', 'nativeNX')
    if not os.path.isdir(native):
        sys.exit('The game files were not found in %s. Pass the recomp folder (the one holding data/ and mods/): '
                 'python3 tools/make_arcs.py <recomp folder>' % os.path.join(recomp, 'data'))
    for lang in LANGS:
        for rel, layout in ARCS:
            rel = rel % lang
            src = os.path.join(native, lang, rel)
            out = patch_arc(open(src, 'rb').read(), layout)
            if out is None:
                print('no', layout, 'in', src)
                continue
            dst = os.path.join(mod, 'romfs', 'nativeNX', lang, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, 'wb').write(out)
            print('wrote', dst)
        village = os.path.join(native, lang, 'arc', 'village', f'common_{lang}.arc')
        with open(village, 'rb') as archive:
            for rel, message in camp_message_files(archive.read(), lang):
                dst = os.path.join(mod, rel)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                with open(dst, 'wb') as out:
                    out.write(message)


if __name__ == '__main__':
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # the mod folder (holding tools/)
    recomp = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(here))  # (mods/field-qol/..)
    main(recomp, sys.argv[2] if len(sys.argv) > 2 else here)
    print('Done: start the game and the reward screen has "Send all to Item Box & Exit".')
