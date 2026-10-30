#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Fileover. Part of cps2recomp.
"""
Validate the ROM zips in a folder for cps2recomp (https://github.com/tmcleroy/cps2recomp).

The launcher does this check itself when it opens, on the `roms` folder beside it (launcher/src/romcheck.cpp, which
is this script's checks in C++); this one is for a command line, and for the extras a zip can be cleaned of.

It repeats what the cps2recomp launcher / games do when they read a ROM set:
  * every file of a set is looked up by CRC32 (the internal file name does not matter);
  * files are searched in the chosen zip first, then in <set>.zip, <parent>.zip and the zips of the game's other
    regions beside it;
  * a game plays its main set, or else another region (game.cfg's other_set: same code, own program files and key);
    a zip named for a region is read as that region first, and is checked on its own;
  * the cipher key file (<set>.key) counts as a required file (CPS-1 games have none);
  * only Store / Deflate zip entries can be read by the launcher.
Files inside the zip that no region of the game uses are reported as extras (a warning only: the launcher ignores
them). After the report, for each zip with extras it asks "Remove them? [y/N]": yes rewrites the zip without them,
anything else leaves the zip untouched. Files that another valid set still takes from that zip are never removed.

The checksum table below comes from the games/*/game.cfg files of the repository.

Usage:
    python validate_cps2_roms.py              # validate the zips in the current folder
    python validate_cps2_roms.py -d D:\\roms   # another folder
    python validate_cps2_roms.py -v           # also list every file of every set
    python validate_cps2_roms.py -q           # quick: trust the CRC32 stored in the zip directory
    python validate_cps2_roms.py --list       # show the supported sets
    python validate_cps2_roms.py --no-pause   # do not wait for Enter at the end (the pause only happens in a console)
    python validate_cps2_roms.py --no-clean   # only report extras, never ask to remove them
    python validate_cps2_roms.py -y           # remove extras from every zip without asking
    python validate_cps2_roms.py --backup     # keep <name>.zip.bak when a zip is rewritten

Exit code: 0 = every set found is valid, 1 = at least one set is invalid, 2 = no zip files found.
"""

import argparse
import os
import shutil
import sys
import zipfile
import zlib

# Generated from the games/*/game.cfg files of cps2recomp (commit e6142dc, 2026-10-29).
# set id -> parent set, display name and files as (kind, file name, CRC32).
# "others" (game.cfg's other_set): other regions the same port plays; each has its own program files and key, and the
# graphics / sound / samples of the set. Vampire Savior Definitive is built from two of these sets, vsavj and vsav2.
SETS = {
    "avspu": {
        "parent": "avsp",
        "name": "Alien vs. Predator (USA 940520)",
        "files": [
            ("program", "avpu.03d", 0x42757950),
            ("program", "avpu.04d", 0x5abcdee6),
            ("program", "avp.05d", 0xfbfb5d7a),
            ("program", "avp.06", 0x190b817f),
            ("graphics", "avp.13m", 0x8f8b5ae4),
            ("graphics", "avp.15m", 0xb00280df),
            ("graphics", "avp.17m", 0x94403195),
            ("graphics", "avp.19m", 0xe1981245),
            ("graphics", "avp.14m", 0x39933b1e),
            ("graphics", "avp.16m", 0x85412860),
            ("graphics", "avp.18m", 0x2e2beb06),
            ("graphics", "avp.20m", 0xe4798d1a),
            ("sound_program", "avp.01", 0x2d3b4220),
            ("samples", "avp.11m", 0x83499817),
            ("samples", "avp.12m", 0xf4110d49),
            ("key", "avspu.key", 0x4e68e346),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "avsp",
                "name": "Alien vs. Predator (Europe 940520)",
                "program": [("avpe.03d", 0x774334a9), ("avpe.04d", 0x7fa83769), ("avp.05d", 0xfbfb5d7a), ("avp.06", 0x190b817f)],
                "key": ("avsp.key", 0xe69fa35b),
            },
            {
                "id": "avspj",
                "name": "Alien vs. Predator (Japan 940520)",
                "program": [("avpj.03d", 0x49799119), ("avpj.04d", 0x8cd2bba8), ("avp.05d", 0xfbfb5d7a), ("avp.06", 0x190b817f)],
                "key": ("avspj.key", 0x3d5ccc08),
            },
            {
                "id": "avspa",
                "name": "Alien vs. Predator (Asia 940520)",
                "program": [("avpa.03d", 0x6c1c1858), ("avpa.04d", 0x94f50b0c), ("avp.05d", 0xfbfb5d7a), ("avp.06", 0x190b817f)],
                "key": ("avspa.key", 0x728efc00),
            },
            {
                "id": "avsph",
                "name": "Alien vs. Predator (Hispanic 940520)",
                "program": [("avph.03d", 0x3e440447), ("avph.04d", 0xaf6fc82f), ("avp.05d", 0xfbfb5d7a), ("avp.06", 0x190b817f)],
                "key": ("avsph.key", 0xcae7b680),
            },
        ],
    },
    "dino": {
        "parent": "",
        "name": "Cadillacs and Dinosaurs (World 930201)",
        "files": [
            ("program", "cde_23a.8f", 0x8f4e585e),
            ("program", "cde_22a.7f", 0x9278aa12),
            ("program", "cde_21a.6f", 0x66d23de2),
            ("graphics", "cd-1m.3a", 0x8da4f917),
            ("graphics", "cd-3m.5a", 0x6c40f603),
            ("graphics", "cd-2m.4a", 0x09c8fc2d),
            ("graphics", "cd-4m.6a", 0x637ff38f),
            ("graphics", "cd-5m.7a", 0x470befee),
            ("graphics", "cd-7m.9a", 0x22bfb7a3),
            ("graphics", "cd-6m.8a", 0xe7599ac4),
            ("graphics", "cd-8m.10a", 0x211b4b15),
            ("sound_program", "cd_q.5k", 0x605fdb0b),
            ("samples", "cd-q1.1k", 0x60927775),
            ("samples", "cd-q2.2k", 0x770f4c47),
            ("samples", "cd-q3.3k", 0x2f273ffc),
            ("samples", "cd-q4.4k", 0x2c67821d),
        ],
    },
    "vhunt2": {
        "parent": "",
        "name": "Vampire Hunter 2: Darkstalkers Revenge (Japan 970929)",
        "files": [
            ("program", "vh2j.03a", 0x9ae8f186),
            ("program", "vh2j.04a", 0xe2fabf53),
            ("program", "vh2j.05", 0xde34f624),
            ("program", "vh2j.06", 0x6a3b9897),
            ("program", "vh2j.07", 0xb021c029),
            ("program", "vh2j.08", 0xac873dff),
            ("program", "vh2j.09", 0xeaefce9c),
            ("program", "vh2j.10", 0x11730952),
            ("graphics", "vh2.13m", 0x3b02ddaa),
            ("graphics", "vh2.15m", 0x4e40de66),
            ("graphics", "vh2.17m", 0xb31d00c9),
            ("graphics", "vh2.19m", 0x149be3ab),
            ("graphics", "vh2.14m", 0xcd09bd63),
            ("graphics", "vh2.16m", 0xe0182c15),
            ("graphics", "vh2.18m", 0x778dc4f6),
            ("graphics", "vh2.20m", 0x605d9d1d),
            ("sound_program", "vh2.01", 0x67b9f779),
            ("sound_program", "vh2.02", 0xaaf15fcb),
            ("samples", "vh2.11m", 0x38922efd),
            ("samples", "vh2.12m", 0x6e2430af),
            ("key", "vhunt2.key", 0x61306b20),
        ],
    },
    "vsavj": {
        "parent": "vsav",
        "name": "Vampire Savior: The Lord of Vampire (Japan 970519)",
        "files": [
            ("program", "vm3j.03d", 0x2a2e74a4),
            ("program", "vm3j.04d", 0x1c2427bc),
            ("program", "vm3j.05a", 0x95ce88d5),
            ("program", "vm3j.06b", 0x2c4297e0),
            ("program", "vm3j.07b", 0xa38aaae7),
            ("program", "vm3j.08a", 0x5773e5c9),
            ("program", "vm3j.09b", 0xd064f8b9),
            ("program", "vm3j.10b", 0x434518e9),
            ("graphics", "vm3.13m", 0xfd8a11eb),
            ("graphics", "vm3.15m", 0xdd1e7d4e),
            ("graphics", "vm3.17m", 0x6b89445e),
            ("graphics", "vm3.19m", 0x3830fdc7),
            ("graphics", "vm3.14m", 0xc1a28e6c),
            ("graphics", "vm3.16m", 0x194a7304),
            ("graphics", "vm3.18m", 0xdf9a9f47),
            ("graphics", "vm3.20m", 0xc22fc3d9),
            ("sound_program", "vm3.01", 0xf778769b),
            ("sound_program", "vm3.02", 0xcc09faa1),
            ("samples", "vm3.11m", 0xe80e956e),
            ("samples", "vm3.12m", 0x9cd71557),
            ("key", "vsavj.key", 0x36d28ab8),
        ],
    },
    "vsav2": {
        "parent": "",
        "name": "Vampire Savior 2: The Lord of Vampire (Japan 970913)",
        "files": [
            ("program", "vs2j.03", 0x89fd86b4),
            ("program", "vs2j.04", 0x107c091b),
            ("program", "vs2j.05", 0x61979638),
            ("program", "vs2j.06", 0xf37c5bc2),
            ("program", "vs2j.07", 0x8f885809),
            ("program", "vs2j.08", 0x2018c120),
            ("program", "vs2j.09", 0xfac3c217),
            ("program", "vs2j.10", 0xeb490213),
            ("graphics", "vs2.13m", 0x5c852f52),
            ("graphics", "vs2.15m", 0xa20f58af),
            ("graphics", "vs2.17m", 0x39db59ad),
            ("graphics", "vs2.19m", 0x00c763a7),
            ("graphics", "vs2.14m", 0xcd09bd63),
            ("graphics", "vs2.16m", 0xe0182c15),
            ("graphics", "vs2.18m", 0x778dc4f6),
            ("graphics", "vs2.20m", 0x605d9d1d),
            ("sound_program", "vs2.01", 0x35190139),
            ("sound_program", "vs2.02", 0xc32dba09),
            ("samples", "vs2.11m", 0xd67e47b7),
            ("samples", "vs2.12m", 0x6d020a14),
            ("key", "vsav2.key", 0x289028ce),
        ],
    },
    "msh": {
        "parent": "",
        "name": "Marvel Super Heroes (Europe 951024)",
        "files": [
            ("program", "mshe.03e", 0xbd951414),
            ("program", "mshe.04e", 0x19dd42f2),
            ("program", "msh.05", 0x6a091b9e),
            ("program", "msh.06b", 0x803e3fa4),
            ("program", "msh.07a", 0xc45f8e27),
            ("program", "msh.08a", 0x9ca6f12c),
            ("program", "msh.09a", 0x82ec27af),
            ("program", "msh.10b", 0x8d931196),
            ("graphics", "msh.13m", 0x09d14566),
            ("graphics", "msh.15m", 0xee962057),
            ("graphics", "msh.17m", 0x604ece14),
            ("graphics", "msh.19m", 0x94a731e8),
            ("graphics", "msh.14m", 0x4197973e),
            ("graphics", "msh.16m", 0x438da4a0),
            ("graphics", "msh.18m", 0x4db92d94),
            ("graphics", "msh.20m", 0xa2b0c6c0),
            ("sound_program", "msh.01", 0xc976e6f9),
            ("sound_program", "msh.02", 0xce67d0d9),
            ("samples", "msh.11m", 0x37ac6d30),
            ("samples", "msh.12m", 0xde092570),
            ("key", "msh.key", 0xb494368e),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "mshu",
                "name": "Marvel Super Heroes (USA 951024)",
                "program": [("mshu.03", 0xd2805bdd), ("mshu.04", 0x743f96ff), ("msh.05", 0x6a091b9e), ("msh.06b", 0x803e3fa4), ("msh.07a", 0xc45f8e27), ("msh.08a", 0x9ca6f12c), ("msh.09a", 0x82ec27af), ("msh.10b", 0x8d931196)],
                "key": ("mshu.key", 0x745c1bee),
            },
            {
                "id": "mshjr1",
                "name": "Marvel Super Heroes (Japan 951024)",
                "program": [("mshj.03f", 0xff172fd2), ("mshj.04f", 0xebbb205a), ("msh.05", 0x6a091b9e), ("msh.06b", 0x803e3fa4), ("msh.07a", 0xc45f8e27), ("msh.08a", 0x9ca6f12c), ("msh.09a", 0x82ec27af), ("msh.10b", 0x8d931196)],
                "key": ("mshj.key", 0x888761ac),
            },
            {
                "id": "msha",
                "name": "Marvel Super Heroes (Asia 951024)",
                "program": [("msha.03e", 0xec84ec44), ("msha.04e", 0x098b8503), ("msh.05", 0x6a091b9e), ("msh.06b", 0x803e3fa4), ("msh.07a", 0xc45f8e27), ("msh.08a", 0x9ca6f12c), ("msh.09a", 0x82ec27af), ("msh.10b", 0x8d931196)],
                "key": ("msha.key", 0x00f3f2ca),
            },
            {
                "id": "mshbr1",
                "name": "Marvel Super Heroes (Brazil 951024)",
                "program": [("mshb.03b", 0xeb16165b), ("mshb.04b", 0x94fb3c97), ("msh.05", 0x6a091b9e), ("msh.06b", 0x803e3fa4), ("msh.07a", 0xc45f8e27), ("msh.08a", 0x9ca6f12c), ("msh.09a", 0x82ec27af), ("msh.10b", 0x8d931196)],
                "key": ("mshb.key", 0x92196837),
            },
        ],
    },
    "mshvsf": {
        "parent": "",
        "name": "Marvel Super Heroes Vs. Street Fighter (Europe 970625)",
        "files": [
            ("program", "mvse.03f", 0xb72dc199),
            ("program", "mvse.04f", 0x6ef799f9),
            ("program", "mvs.05a", 0x1a5de0cb),
            ("program", "mvs.06a", 0x959f3030),
            ("program", "mvs.07b", 0x7f915bdb),
            ("program", "mvs.08a", 0xc2813884),
            ("program", "mvs.09b", 0x3ba08818),
            ("program", "mvs.10b", 0xcf0dba98),
            ("graphics", "mvs.13m", 0x29b05fd9),
            ("graphics", "mvs.15m", 0xfaddccf1),
            ("graphics", "mvs.17m", 0x97aaf4c7),
            ("graphics", "mvs.19m", 0xcb70e915),
            ("graphics", "mvs.14m", 0xb3b1972d),
            ("graphics", "mvs.16m", 0x08aadb5d),
            ("graphics", "mvs.18m", 0xc1228b35),
            ("graphics", "mvs.20m", 0x366cc6c2),
            ("sound_program", "mvs.01", 0x68252324),
            ("sound_program", "mvs.02", 0xb34e773d),
            ("samples", "mvs.11m", 0x86219770),
            ("samples", "mvs.12m", 0xf2fd7f68),
            ("key", "mshvsf.key", 0x64660867),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "mshvsfu1",
                "name": "Marvel Super Heroes Vs. Street Fighter (USA 970625)",
                "program": [("mvsu.03d", 0xae60a66a), ("mvsu.04d", 0x91f67d8a), ("mvs.05a", 0x1a5de0cb), ("mvs.06a", 0x959f3030), ("mvs.07b", 0x7f915bdb), ("mvs.08a", 0xc2813884), ("mvs.09b", 0x3ba08818), ("mvs.10b", 0xcf0dba98)],
                "key": ("mshvsfu.key", 0x4c04797b),
            },
            {
                "id": "mshvsfj2",
                "name": "Marvel Super Heroes Vs. Street Fighter (Japan 970625)",
                "program": [("mvsj.03g", 0xfdfa7e26), ("mvsj.04g", 0xc921825f), ("mvs.05a", 0x1a5de0cb), ("mvs.06a", 0x959f3030), ("mvs.07b", 0x7f915bdb), ("mvs.08a", 0xc2813884), ("mvs.09b", 0x3ba08818), ("mvs.10b", 0xcf0dba98)],
                "key": ("mshvsfj.key", 0x565eeebb),
            },
            {
                "id": "mshvsfh",
                "name": "Marvel Super Heroes Vs. Street Fighter (Hispanic 970625)",
                "program": [("mvsh.03f", 0x4f60f41e), ("mvsh.04f", 0xdc08ec12), ("mvs.05a", 0x1a5de0cb), ("mvs.06a", 0x959f3030), ("mvs.07b", 0x7f915bdb), ("mvs.08a", 0xc2813884), ("mvs.09b", 0x3ba08818), ("mvs.10b", 0xcf0dba98)],
                "key": ("mshvsfh.key", 0xb93d576f),
            },
            {
                "id": "mshvsfa",
                "name": "Marvel Super Heroes Vs. Street Fighter (Asia 970625)",
                "program": [("mvsa.03f", 0x5b863716), ("mvsa.04f", 0x4886e65f), ("mvs.05a", 0x1a5de0cb), ("mvs.06a", 0x959f3030), ("mvs.07b", 0x7f915bdb), ("mvs.08a", 0xc2813884), ("mvs.09b", 0x3ba08818), ("mvs.10b", 0xcf0dba98)],
                "key": ("mshvsfa.key", 0x6810a3af),
            },
            {
                "id": "mshvsfb1",
                "name": "Marvel Super Heroes Vs. Street Fighter (Brazil 970625)",
                "program": [("mvsb.03f", 0x9c4bb950), ("mvsb.04f", 0xd3320d13), ("mvs.05a", 0x1a5de0cb), ("mvs.06a", 0x959f3030), ("mvs.07b", 0x7f915bdb), ("mvs.08a", 0xc2813884), ("mvs.09b", 0x3ba08818), ("mvs.10b", 0xcf0dba98)],
                "key": ("mshvsfb.key", 0x3f5bb6e4),
            },
        ],
    },
    "mvsc": {
        "parent": "",
        "name": "Marvel Vs. Capcom: Clash of Super Heroes (Europe 980123)",
        "files": [
            ("program", "mvce.03a", 0x824e4a90),
            ("program", "mvce.04a", 0x436c5a4e),
            ("program", "mvc.05a", 0x2d8c8e86),
            ("program", "mvc.06a", 0x8528e1f5),
            ("program", "mvc.07", 0xc3baa32b),
            ("program", "mvc.08", 0xbc002fcd),
            ("program", "mvc.09", 0xc67b26df),
            ("program", "mvc.10", 0x0fdd1e26),
            ("graphics", "mvc.13m", 0xfa5f74bc),
            ("graphics", "mvc.15m", 0x71938a8f),
            ("graphics", "mvc.17m", 0x92741d07),
            ("graphics", "mvc.19m", 0xbcb72fc6),
            ("graphics", "mvc.14m", 0x7f1df4e4),
            ("graphics", "mvc.16m", 0x90bd3203),
            ("graphics", "mvc.18m", 0x67aaf727),
            ("graphics", "mvc.20m", 0x8b0bade8),
            ("sound_program", "mvc.01", 0x41629e95),
            ("sound_program", "mvc.02", 0x963abf6b),
            ("samples", "mvc.11m", 0x850fe663),
            ("samples", "mvc.12m", 0x7ccb1896),
            ("key", "mvsc.key", 0x7e101e09),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "mvscu",
                "name": "Marvel Vs. Capcom: Clash of Super Heroes (USA 980123)",
                "program": [("mvcu.03d", 0xc6007557), ("mvcu.04d", 0x724b2b20), ("mvc.05a", 0x2d8c8e86), ("mvc.06a", 0x8528e1f5), ("mvc.07", 0xc3baa32b), ("mvc.08", 0xbc002fcd), ("mvc.09", 0xc67b26df), ("mvc.10", 0x0fdd1e26)],
                "key": ("mvscu.key", 0xa83db333),
            },
            {
                "id": "mvscj",
                "name": "Marvel Vs. Capcom: Clash of Super Heroes (Japan 980123)",
                "program": [("mvcj.03a", 0x3df18879), ("mvcj.04a", 0x07d212e8), ("mvc.05a", 0x2d8c8e86), ("mvc.06a", 0x8528e1f5), ("mvc.07", 0xc3baa32b), ("mvc.08", 0xbc002fcd), ("mvc.09", 0xc67b26df), ("mvc.10", 0x0fdd1e26)],
                "key": ("mvscj.key", 0x9dedbcaf),
            },
            {
                "id": "mvsca",
                "name": "Marvel Vs. Capcom: Clash of Super Heroes (Asia 980123)",
                "program": [("mvca.03a", 0x2ff4ae25), ("mvca.04a", 0xf28427ef), ("mvc.05a", 0x2d8c8e86), ("mvc.06a", 0x8528e1f5), ("mvc.07", 0xc3baa32b), ("mvc.08", 0xbc002fcd), ("mvc.09", 0xc67b26df), ("mvc.10", 0x0fdd1e26)],
                "key": ("mvsca.key", 0x31edaee8),
            },
            {
                "id": "mvsch",
                "name": "Marvel Vs. Capcom: Clash of Super Heroes (Hispanic 980123)",
                "program": [("mvch.03", 0x6a0ec9f7), ("mvch.04", 0x00f03fa4), ("mvc.05a", 0x2d8c8e86), ("mvc.06a", 0x8528e1f5), ("mvc.07", 0xc3baa32b), ("mvc.08", 0xbc002fcd), ("mvc.09", 0xc67b26df), ("mvc.10", 0x0fdd1e26)],
                "key": ("mvsch.key", 0xdd647c0d),
            },
            {
                "id": "mvscb",
                "name": "Marvel Vs. Capcom: Clash of Super Heroes (Brazil 980123)",
                "program": [("mvcb.03a", 0x7155953b), ("mvcb.04a", 0xfb117d0e), ("mvc.05a", 0x2d8c8e86), ("mvc.06a", 0x8528e1f5), ("mvc.07", 0xc3baa32b), ("mvc.08", 0xbc002fcd), ("mvc.09", 0xc67b26df), ("mvc.10", 0x0fdd1e26)],
                "key": ("mvscb.key", 0xd74a7a3d),
            },
        ],
    },
    "xmvsf": {
        "parent": "",
        "name": "X-Men Vs. Street Fighter (Europe 961004)",
        "files": [
            ("program", "xvse.03f", 0xdb06413f),
            ("program", "xvse.04f", 0xef015aef),
            ("program", "xvs.05a", 0x7db6025d),
            ("program", "xvs.06a", 0xe8e2c75c),
            ("program", "xvs.07", 0x08f0abed),
            ("program", "xvs.08", 0x81929675),
            ("program", "xvs.09", 0x9641f36b),
            ("graphics", "xvs.13m", 0xf6684efd),
            ("graphics", "xvs.15m", 0x29109221),
            ("graphics", "xvs.17m", 0x92db3474),
            ("graphics", "xvs.19m", 0x3733473c),
            ("graphics", "xvs.14m", 0xbcac2e41),
            ("graphics", "xvs.16m", 0xea04a272),
            ("graphics", "xvs.18m", 0xb0def86a),
            ("graphics", "xvs.20m", 0x4b40ff9f),
            ("sound_program", "xvs.01", 0x3999e93a),
            ("sound_program", "xvs.02", 0x101bdee9),
            ("samples", "xvs.11m", 0x9cadcdbc),
            ("samples", "xvs.12m", 0x7b11e460),
            ("key", "xmvsf.key", 0xd5c07311),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "xmvsfur1",
                "name": "X-Men Vs. Street Fighter (USA 961004)",
                "program": [("xvsu.03i", 0x5481155a), ("xvsu.04i", 0x1e236388), ("xvs.05a", 0x7db6025d), ("xvs.06a", 0xe8e2c75c), ("xvs.07", 0x08f0abed), ("xvs.08", 0x81929675), ("xvs.09", 0x9641f36b)],
                "key": ("xmvsfu.key", 0xeca13458),
            },
            {
                "id": "xmvsfjr2",
                "name": "X-Men Vs. Street Fighter (Japan 961004)",
                "program": [("xvsj.03i", 0xef24da96), ("xvsj.04i", 0x70a59b35), ("xvs.05a", 0x7db6025d), ("xvs.06a", 0xe8e2c75c), ("xvs.07", 0x08f0abed), ("xvs.08", 0x81929675), ("xvs.09", 0x9641f36b)],
                "key": ("xmvsfj.key", 0x87576cda),
            },
            {
                "id": "xmvsfar1",
                "name": "X-Men Vs. Street Fighter (Asia 961004)",
                "program": [("xvsa.03", 0x520054df), ("xvsa.04", 0x13086e55), ("xvs.05a", 0x7db6025d), ("xvs.06a", 0xe8e2c75c), ("xvs.07", 0x08f0abed), ("xvs.08", 0x81929675), ("xvs.09", 0x9641f36b)],
                "key": ("xmvsfa.key", 0x44941468),
            },
            {
                "id": "xmvsfh",
                "name": "X-Men Vs. Street Fighter (Hispanic 961004)",
                "program": [("xvsh.03a", 0xd4fffb04), ("xvsh.04a", 0x1b4ea638), ("xvs.05a", 0x7db6025d), ("xvs.06a", 0xe8e2c75c), ("xvs.07", 0x08f0abed), ("xvs.08", 0x81929675), ("xvs.09", 0x9641f36b)],
                "key": ("xmvsfh.key", 0xf632a36b),
            },
        ],
    },
    "xmcota": {
        "parent": "",
        "name": "X-Men: Children of the Atom (Euro 950331)",
        "files": [
            ("program", "xmne.03f", 0x5a726d13),
            ("program", "xmne.04f", 0x06a83f3a),
            ("program", "xmne.05b", 0x87b0ed0f),
            ("program", "xmn.06a", 0x1b86a328),
            ("program", "xmn.07a", 0x2c142a44),
            ("program", "xmn.08a", 0xf712d44f),
            ("program", "xmn.09a", 0x9241cae8),
            ("program", "xmne.10b", 0xcb36b0a4),
            ("graphics", "xmn.13m", 0xbf4df073),
            ("graphics", "xmn.15m", 0x4d7e4cef),
            ("graphics", "xmn.17m", 0x513eea17),
            ("graphics", "xmn.19m", 0xd23897fc),
            ("graphics", "xmn.14m", 0x778237b7),
            ("graphics", "xmn.16m", 0x67b36948),
            ("graphics", "xmn.18m", 0x015a7c4c),
            ("graphics", "xmn.20m", 0x9dde2758),
            ("sound_program", "xmn.01a", 0x40f479ea),
            ("sound_program", "xmn.02a", 0x39d9b5ad),
            ("samples", "xmn.11m", 0xc848a6bc),
            ("samples", "xmn.12m", 0x729c188f),
            ("key", "xmcota.key", 0x6665bbfb),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "xmcotah",
                "name": "X-Men: Children of the Atom (Hispanic 950331)",
                "program": [("xmnh.03f", 0xe4b85a90), ("xmnh.04f", 0x7dfe1406), ("xmnh.05b", 0x87b0ed0f), ("xmn.06a", 0x1b86a328), ("xmn.07a", 0x2c142a44), ("xmn.08a", 0xf712d44f), ("xmn.09a", 0x9241cae8), ("xmnh.10b", 0xcb36b0a4)],
                "key": ("xmcotah.key", 0xc9a45a5a),
            },
            {
                "id": "xmcotab",
                "name": "X-Men: Children of the Atom (Brazil 950331)",
                "program": [("xmnb.03c", 0xab48bcb0), ("xmnb.04c", 0x8d8fcbb1), ("xmne.05b", 0x87b0ed0f), ("xmn.06a", 0x1b86a328), ("xmn.07a", 0x2c142a44), ("xmn.08a", 0xf712d44f), ("xmn.09a", 0x9241cae8), ("xmne.10b", 0xcb36b0a4)],
                "key": ("xmcotab.key", 0xbb70f10f),
            },
        ],
    },
    "sfa": {
        "parent": "",
        "name": "Street Fighter Alpha: Warriors' Dreams (Europe 950727)",
        "files": [
            ("program", "sfze.03d", 0xebf2054d),
            ("program", "sfz.04b", 0x8b73b0e5),
            ("program", "sfz.05a", 0x0810544d),
            ("program", "sfz.06", 0x806e8f38),
            ("graphics", "sfz.14m", 0x90fefdb3),
            ("graphics", "sfz.16m", 0x5354c948),
            ("graphics", "sfz.18m", 0x41a1e790),
            ("graphics", "sfz.20m", 0xa549df98),
            ("sound_program", "sfz.01", 0xffffec7d),
            ("sound_program", "sfz.02", 0x45f46a08),
            ("samples", "sfz.11m", 0xc4b093cd),
            ("samples", "sfz.12m", 0x8bdbc4b4),
            ("key", "sfa.key", 0x7c095631),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "sfzbr1",
                "name": "Street Fighter Zero (Brazil 950727)",
                "program": [("sfzb.03e", 0xecba89a3), ("sfz.04b", 0x8b73b0e5), ("sfz.05a", 0x0810544d), ("sfz.06", 0x806e8f38)],
                "key": ("sfzb.key", 0xb0570359),
            },
        ],
    },
    "sfa2u": {
        "parent": "sfa2",
        "name": "Street Fighter Alpha 2 (USA 960430)",
        "files": [
            ("program", "sz2u.03a", 0xd03e504f),
            ("program", "sz2u.04a", 0xfae0e9c3),
            ("program", "sz2u.05a", 0xd02dd758),
            ("program", "sz2u.06", 0xc5c8eb63),
            ("program", "sz2u.07", 0x5de01cc5),
            ("program", "sz2u.08", 0xbea11d56),
            ("graphics", "sz2.13m", 0x4d1f1f22),
            ("graphics", "sz2.15m", 0x19cea680),
            ("graphics", "sz2.17m", 0xe01b4588),
            ("graphics", "sz2.19m", 0x0feeda64),
            ("graphics", "sz2.14m", 0x0560c6aa),
            ("graphics", "sz2.16m", 0xae940f87),
            ("graphics", "sz2.18m", 0x4bc3c8bc),
            ("graphics", "sz2.20m", 0x39e674c0),
            ("sound_program", "sz2.01a", 0x1bc323cf),
            ("sound_program", "sz2.02a", 0xba6a5013),
            ("samples", "sz2.11m", 0xaa47a601),
            ("samples", "sz2.12m", 0x2237bc53),
            ("key", "sfa2u.key", 0x4a8d91ef),
        ],
    },
    "sfa3u": {
        "parent": "sfa3",
        "name": "Street Fighter Alpha 3 (USA 980904)",
        "files": [
            ("program", "sz3u.03c", 0xe007da2e),
            ("program", "sz3u.04c", 0x5f78f0e7),
            ("program", "sz3.05c", 0x57fd0a40),
            ("program", "sz3.06c", 0xf6305f8b),
            ("program", "sz3.07c", 0x6eab0f6f),
            ("program", "sz3.08c", 0x910c4a3b),
            ("program", "sz3.09c", 0xb29e5199),
            ("program", "sz3.10b", 0xdeb2ff52),
            ("graphics", "sz3.13m", 0x0f7a60d9),
            ("graphics", "sz3.15m", 0x8e933741),
            ("graphics", "sz3.17m", 0xd6e98147),
            ("graphics", "sz3.19m", 0xf31a728a),
            ("graphics", "sz3.14m", 0x5ff98297),
            ("graphics", "sz3.16m", 0x52b5bdee),
            ("graphics", "sz3.18m", 0x40631ed5),
            ("graphics", "sz3.20m", 0x763409b4),
            ("sound_program", "sz3.01", 0xde810084),
            ("sound_program", "sz3.02", 0x72445dc4),
            ("samples", "sz3.11m", 0x1c89eed1),
            ("samples", "sz3.12m", 0xf392b13a),
            ("key", "sfa3u.key", 0x4a8f98c1),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "sfa3",
                "name": "Street Fighter Alpha 3 (Europe 980904)",
                "program": [("sz3e.03c", 0x9762b206), ("sz3e.04c", 0x5ad3f721), ("sz3.05c", 0x57fd0a40), ("sz3.06c", 0xf6305f8b), ("sz3.07c", 0x6eab0f6f), ("sz3.08c", 0x910c4a3b), ("sz3.09c", 0xb29e5199), ("sz3.10b", 0xdeb2ff52)],
                "key": ("sfa3.key", 0x54fa39c6),
            },
            {
                "id": "sfz3a",
                "name": "Street Fighter Zero 3 (Asia 980904)",
                "program": [("sz3a.03d", 0xd7e140d6), ("sz3a.04d", 0xe06869a2), ("sz3.05c", 0x57fd0a40), ("sz3.06c", 0xf6305f8b), ("sz3.07c", 0x6eab0f6f), ("sz3.08c", 0x910c4a3b), ("sz3.09c", 0xb29e5199), ("sz3.10b", 0xdeb2ff52)],
                "key": ("sfz3a.key", 0x09045d61),
            },
            {
                "id": "sfa3h",
                "name": "Street Fighter Alpha 3 (Hispanic 980904)",
                "program": [("sz3h.03c", 0xb3b563a3), ("sz3h.04c", 0x47891fec), ("sz3.05c", 0x57fd0a40), ("sz3.06c", 0xf6305f8b), ("sz3.07c", 0x6eab0f6f), ("sz3.08c", 0x910c4a3b), ("sz3.09c", 0xb29e5199), ("sz3.10b", 0xdeb2ff52)],
                "key": ("sfa3h.key", 0x1b34998c),
            },
        ],
    },
    "sfz2al": {
        "parent": "",
        "name": "Street Fighter Zero 2 Alpha (Asia 960826)",
        "files": [
            ("program", "szaa.03", 0x88e7023e),
            ("program", "szaa.04", 0xae8ec36e),
            ("program", "szaa.05", 0xf053a55e),
            ("program", "szaa.06", 0xcfc0e7a8),
            ("program", "szaa.07", 0x5feb8b20),
            ("program", "szaa.08", 0x6eb6d412),
            ("graphics", "sza.13m", 0x4d1f1f22),
            ("graphics", "sza.15m", 0x19cea680),
            ("graphics", "sza.17m", 0xe01b4588),
            ("graphics", "sza.19m", 0x0feeda64),
            ("graphics", "sza.14m", 0x0560c6aa),
            ("graphics", "sza.16m", 0xae940f87),
            ("graphics", "sza.18m", 0x4bc3c8bc),
            ("graphics", "sza.20m", 0x39e674c0),
            ("sound_program", "sza.01", 0x1bc323cf),
            ("sound_program", "sza.02", 0xba6a5013),
            ("samples", "sza.11m", 0xaa47a601),
            ("samples", "sza.12m", 0x2237bc53),
            ("key", "sfz2al.key", 0x2904963e),
        ],
    },
    "ssf2xjr1": {
        "parent": "ssf2t",
        "name": "Super Street Fighter II X: Grand Master Challenge (Japan 940223)",
        "files": [
            ("program", "sfxj.03c", 0xa7417b79),
            ("program", "sfxj.04a", 0xaf7767b4),
            ("program", "sfxj.05", 0xf4ff18f5),
            ("program", "sfxj.06a", 0x260d0370),
            ("program", "sfxj.07", 0x1324d02a),
            ("program", "sfxj.08", 0x2de76f10),
            ("program", "sfx.09", 0x642fae3f),
            ("graphics", "sfx.13m", 0xcf94d275),
            ("graphics", "sfx.15m", 0x5eb703af),
            ("graphics", "sfx.17m", 0xffa60e0f),
            ("graphics", "sfx.19m", 0x34e825c5),
            ("graphics", "sfx.14m", 0xb7cc32e7),
            ("graphics", "sfx.16m", 0x8376ad18),
            ("graphics", "sfx.18m", 0xf5b1b336),
            ("graphics", "sfx.20m", 0x459d5c6b),
            ("graphics", "sfx.21m", 0xe32854af),
            ("graphics", "sfx.23m", 0x760f2927),
            ("graphics", "sfx.25m", 0x1ee90208),
            ("graphics", "sfx.27m", 0xf814400f),
            ("sound_program", "sfx.01", 0xb47b8835),
            ("sound_program", "sfx.02", 0x0022633f),
            ("samples", "sfx.11m", 0x9bdbd476),
            ("samples", "sfx.12m", 0xa05e3aab),
            ("key", "ssf2xj.key", 0x160d1424),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "ssf2ta",
                "name": "Super Street Fighter II Turbo (Asia 940223)",
                "program": [("sfxa.03c", 0x04b9ff34), ("sfxa.04a", 0x16ea5f7a), ("sfxa.05", 0x53d61f0c), ("sfxa.06a", 0x066d09b5), ("sfxa.07", 0xa428257b), ("sfxa.08", 0x39be596c), ("sfx.09", 0x642fae3f)],
                "key": ("ssf2ta.key", 0xc11fa8e9),
            },
            {
                "id": "ssf2xj",
                "name": "Super Street Fighter II X: Grand Master Challenge (Japan 940311)",
                "program": [("sfxj.03d", 0x50b52b37), ("sfxj.04a", 0xaf7767b4), ("sfxj.05", 0xf4ff18f5), ("sfxj.06b", 0x413477c2), ("sfxj.07a", 0xa18b3d83), ("sfxj.08", 0x2de76f10), ("sfx.09", 0x642fae3f)],
                "key": ("ssf2xj.key", 0x160d1424),
            },
        ],
    },
    "sgemf": {
        "parent": "",
        "name": "Super Gem Fighter: Mini Mix (USA 970904)",
        "files": [
            ("program", "pcfu.03", 0xac2e8566),
            ("program", "pcf.04", 0xf4314c96),
            ("program", "pcf.05", 0x215655f6),
            ("program", "pcf.06", 0xea6f13ea),
            ("program", "pcf.07", 0x5ac6d5ea),
            ("graphics", "pcf.13m", 0x22d72ab9),
            ("graphics", "pcf.15m", 0x16a4813c),
            ("graphics", "pcf.17m", 0x1097e035),
            ("graphics", "pcf.19m", 0xd362d874),
            ("graphics", "pcf.14m", 0x0383897c),
            ("graphics", "pcf.16m", 0x76f91084),
            ("graphics", "pcf.18m", 0x756c3754),
            ("graphics", "pcf.20m", 0x9ec9277d),
            ("sound_program", "pcf.01", 0x254e5f33),
            ("sound_program", "pcf.02", 0x6902f4f9),
            ("samples", "pcf.11m", 0xa5dea005),
            ("samples", "pcf.12m", 0x4ce235fe),
            ("key", "sgemf.key", 0x3d604021),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "pfghtj",
                "name": "Pocket Fighter (Japan 970904)",
                "program": [("pcfj.03", 0x681da43e), ("pcf.04", 0xf4314c96), ("pcf.05", 0x215655f6), ("pcf.06", 0xea6f13ea), ("pcf.07", 0x5ac6d5ea)],
                "key": ("pfghtj.key", 0x62297638),
            },
            {
                "id": "sgemfa",
                "name": "Super Gem Fighter: Mini Mix (Asia 970904)",
                "program": [("pcfa.03", 0xe17c089a), ("pcf.04", 0xf4314c96), ("pcf.05", 0x215655f6), ("pcf.06", 0xea6f13ea), ("pcf.07", 0x5ac6d5ea)],
                "key": ("sgemfa.key", 0xdd513738),
            },
            {
                "id": "sgemfh",
                "name": "Super Gem Fighter: Mini Mix (Hispanic 970904)",
                "program": [("pcfh.03", 0xe9103347), ("pcf.04", 0xf4314c96), ("pcf.05", 0x215655f6), ("pcf.06", 0xea6f13ea), ("pcf.07", 0x5ac6d5ea)],
                "key": ("sgemfh.key", 0xf97f4b7d),
            },
        ],
    },
    "spf2t": {
        "parent": "",
        "name": "Super Puzzle Fighter II Turbo (Europe 960529)",
        "files": [
            ("program", "pzfe.03", 0x2af51954),
            ("program", "pzf.04", 0xb80649e2),
            ("graphics", "pzf.14m", 0x2d4881cb),
            ("graphics", "pzf.16m", 0x4b0fd1be),
            ("graphics", "pzf.18m", 0xe43aac33),
            ("graphics", "pzf.20m", 0x7f536ff1),
            ("sound_program", "pzf.01", 0x600fb2a3),
            ("sound_program", "pzf.02", 0x496076e0),
            ("samples", "pzf.11m", 0x78442743),
            ("samples", "pzf.12m", 0x399d2c7b),
            ("key", "spf2t.key", 0x4c4dc7e3),
        ],
        "others": [  # other regions: their own program files and key, the rest is the set's
            {
                "id": "spf2ta",
                "name": "Super Puzzle Fighter II Turbo (Asia 960529)",
                "program": [("pzfa.03", 0x3cecfa78), ("pzf.04", 0xb80649e2)],
                "key": ("spf2ta.key", 0x61e93a18),
            },
        ],
    },
}

CHUNK = 1 << 20
READABLE_METHODS = (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED)  # the only ones the launcher can unpack


class Colors:
    def __init__(self, enabled):
        self.ok = "\033[32m" if enabled else ""
        self.fail = "\033[31m" if enabled else ""
        self.warn = "\033[33m" if enabled else ""
        self.dim = "\033[2m" if enabled else ""
        self.end = "\033[0m" if enabled else ""

    def paint(self, color, text):
        return f"{color}{text}{self.end}"


class Archive:
    """A zip file opened once; entries are verified (fully unpacked) at most once."""

    def __init__(self, path):
        self.path = path
        self.name = os.path.basename(path)
        self.zip = zipfile.ZipFile(path)
        self.entries = [i for i in self.zip.infolist() if not i.is_dir()]
        self._checked = {}

    def close(self):
        self.zip.close()

    def verify(self, info):
        """None if the entry unpacks cleanly (the zip module checks the CRC32 at the end), else a message."""
        if info.compress_type not in READABLE_METHODS:
            return (f"uses zip compression method {info.compress_type}, which the launcher cannot read "
                    "(re-zip it with Deflate or Store)")
        key = info.header_offset
        if key not in self._checked:
            try:
                with self.zip.open(info) as stream:
                    while stream.read(CHUNK):
                        pass
                self._checked[key] = None
            except (zipfile.BadZipFile, zlib.error, EOFError, NotImplementedError, RuntimeError, OSError) as exc:
                self._checked[key] = f"cannot be unpacked or is corrupt ({exc})"
        return self._checked[key]


def open_archive(path, cache):
    """An Archive, or an error string; cached."""
    if path not in cache:
        try:
            cache[path] = Archive(path)
        except (zipfile.BadZipFile, OSError) as exc:
            cache[path] = str(exc) or exc.__class__.__name__
    return cache[path]


def base_name(entry_name):
    return entry_name.replace("\\", "/").rsplit("/", 1)[-1]


def zip_names(set_id, spec):
    """Names of the zips that can hold a game, as the launcher looks for them: the set, its parent, its other regions."""
    names = [set_id, spec["parent"]] + [other["id"] for other in spec.get("others", [])]
    return list(dict.fromkeys(name for name in names if name))


def regions(set_id, spec):
    """The ways a game can be read, as (set id, name, files): its own set, then each other region (game.cfg's other_set)."""
    found = [(set_id, spec["name"], spec["files"])]
    for other in spec.get("others", []):
        files = [("program", name, crc) for name, crc in other["program"]]
        files += [f for f in spec["files"] if f[0] in ("graphics", "sound_program", "samples")]
        if other["key"]:
            files.append(("key", other["key"][0], other["key"][1]))
        found.append((other["id"], other["name"], files))
    return found


def check_files(files, chain, quick):
    """Looks every file of one region up by CRC32 in the zips of `chain`. Returns (problems, used)."""
    problems, used = [], []
    for kind, name, crc in files:
        hit = next(((a, i) for a in chain for i in a.entries if i.CRC == crc), None)
        if hit is None:
            near = next((i for a in chain for i in a.entries if base_name(i.filename).lower() == name.lower()), None)
            if near is not None:
                problems.append(f"{name} ({kind}): wrong CRC32, zip has {near.CRC:08x} but {crc:08x} is expected")
            else:
                problems.append(f"{name} ({kind}): missing (CRC32 {crc:08x})")
            continue
        arc, info = hit
        error = None if quick else arc.verify(info)
        if error:
            problems.append(f"{name} ({kind}): {base_name(info.filename)} in {arc.name} {error}")
        else:
            used.append((name, arc.name, base_name(info.filename)))
    return problems, used


def check_candidate(set_id, spec, primary, listing, cache, quick):
    """
    Same lookup as cps2recomp's LoadRomSet with `primary` as the chosen zip: files are found by CRC32 in it, then in
    the zips of the set, its parent and its other regions beside it. The program read is the one of the region the zip
    is named for, else the set's own, else the first other region whose files are all there. Returns
    (problems, used, extras, region): used is a list of (file name, zip name, name inside the zip); region is the
    (set id, name, files) that was read, or None when none could be; extras lists the files of `primary` that no
    region of the game uses, as (name inside the zip, size, CRC32): the other regions' own files are not extras, they
    are ROMs the game plays too. When nothing could be read, problems are those of the region the launcher tries first.
    """
    first = open_archive(primary, cache)
    if isinstance(first, str):
        return [f"{os.path.basename(primary)} is not a readable zip file ({first})"], [], [], None
    chain = [first]
    for name in zip_names(set_id, spec):
        path = listing.get(name.lower() + ".zip")
        if path and all(os.path.normcase(a.path) != os.path.normcase(path) for a in chain):
            other = open_archive(path, cache)
            if not isinstance(other, str):
                chain.append(other)

    chosen = os.path.splitext(os.path.basename(primary))[0].lower()
    options = regions(set_id, spec)
    order = ([r for r in options[1:] if r[0].lower() == chosen] + options[:1]
             + [r for r in options[1:] if r[0].lower() != chosen])
    known = {crc for _, _, files in options for _, _, crc in files}
    extras = [(i.filename, i.file_size, i.CRC) for i in first.entries if i.CRC not in known]
    first_problems = None
    for region in order:
        problems, used = check_files(region[2], chain, quick)
        if not problems:
            return [], used, extras, region
        if first_problems is None:
            first_problems = problems
    return first_problems, [], extras, None


def missing_zip_hints(set_id, spec, listing, tried, is_main=True):
    """Messages about zips that are not in the folder and that the zips tried would need for the files they share."""
    main = [set_id] + ([spec["parent"]] if spec["parent"] else [])
    hints = []
    absent = [n for n in main if n + ".zip" not in listing]
    if is_main and len(main) == 2 and len(absent) == 1:  # only one of <set>.zip / <parent>.zip is here
        role = "this set's zip" if absent[0] == set_id else "the parent set's zip"
        hints.append((absent[0], f"{absent[0]}.zip ({role}) is not in this folder: the set needs both "
                                 f"{main[0]}.zip and {main[1]}.zip, or one merged zip with all of its files"))
    base = spec["parent"] or set_id
    region_ids = {other["id"].lower() for other in spec.get("others", [])}
    for path in tried:
        stem = os.path.splitext(os.path.basename(path))[0]
        if stem.lower() in region_ids and stem.lower() != base and base + ".zip" not in listing \
                and all(base != done for done, _ in hints):
            hints.append((base, f"{base}.zip (the set whose files the {stem} region shares) is not in this folder: "
                                f"{stem}.zip needs it, or one merged zip with all of its files"))
    return [text for _, text in hints]


def rewrite_without_extras(path, drop_crcs, backup):
    """
    Rewrites a zip without the files whose CRC32 is in drop_crcs (the folders that no longer hold a file go too).
    Kept files keep their name, date and compression. The original is replaced only after the new zip passes
    a check. Returns the number of files removed.
    """
    temp = f"{path}.tmp{os.getpid()}"
    removed = 0
    try:
        with zipfile.ZipFile(path) as src, zipfile.ZipFile(temp, "w") as dst:
            infos = src.infolist()
            keep = [i for i in infos if not i.is_dir() and i.CRC not in drop_crcs]
            if not keep:
                raise ValueError("no file would be left in the zip")
            for info in infos:
                new = zipfile.ZipInfo(info.filename, info.date_time)
                new.external_attr = info.external_attr
                if info.is_dir():
                    if any(k.filename.startswith(info.filename) for k in keep):
                        dst.writestr(new, b"")
                    continue
                if info.CRC in drop_crcs:
                    removed += 1
                    continue
                new.compress_type = info.compress_type
                with src.open(info) as reader, dst.open(new, "w", force_zip64=info.file_size > 0x7FFFFFFF) as writer:
                    shutil.copyfileobj(reader, writer, CHUNK)
        with zipfile.ZipFile(temp) as check:
            files = [i for i in check.infolist() if not i.is_dir()]
            if check.testzip() is not None or len(files) != len(keep):
                raise ValueError("the rewritten zip failed its check, the original was not touched")
        if backup:
            shutil.copy2(path, path + ".bak")
        os.replace(temp, path)
    finally:
        if os.path.exists(temp):
            os.remove(temp)
    return removed


def scan_folder(folder):
    """lower-case file name -> full path, for every .zip in the folder."""
    listing = {}
    for entry in sorted(os.listdir(folder)):
        full = os.path.join(folder, entry)
        if entry.lower().endswith(".zip") and os.path.isfile(full):
            listing[entry.lower()] = full
    return listing


def find_lookalike(path, cache):
    """For a zip that no set uses by name: the set whose files it mostly contains, as (set id, matches, total)."""
    arc = open_archive(path, cache)
    if isinstance(arc, str):
        return None
    crcs = {i.CRC for i in arc.entries}
    best = None
    for set_id, spec in SETS.items():
        for region_id, _, files in regions(set_id, spec):
            total = len(files)
            matches = sum(1 for _, _, crc in files if crc in crcs)
            if matches >= 3 and matches * 2 >= total and (best is None or matches > best[1]):
                best = (region_id, matches, total)
    return best


def print_extras(zip_name, extras, colors, out):
    print(colors.paint(colors.warn, f"         + {len(extras)} extra file(s) in {zip_name}, not used by this game:"), file=out)
    for name, size, crc in extras:
        print(colors.paint(colors.warn, f"           {name}  ({size} bytes, CRC32 {crc:08x})"), file=out)


def validate(folder, quick, verbose, colors, out=sys.stdout, clean="no", backup=False, ask=input):
    """clean: "no" only reports extras, "ask" offers to remove them zip by zip, "yes" removes them without asking."""
    listing = scan_folder(folder)
    if not listing:
        print(f"No zip files in {folder}", file=out)
        return 2

    cache = {}
    results = {"ok": 0, "fail": 0, "absent": 0, "extra": 0, "cleaned": 0}
    names_in_use = set()
    primaries = {}  # zip path -> its extra files, for sets that are valid
    needed = {}     # zip path -> CRC32s that some valid set takes from that zip

    for set_id, spec in SETS.items():
        names = zip_names(set_id, spec)
        names_in_use.update(n + ".zip" for n in names)
        main_names = {set_id, spec["parent"]} - {""}
        main_paths = [listing[n + ".zip"] for n in names if n in main_names and n + ".zip" in listing]
        region_paths = [listing[n + ".zip"] for n in names if n not in main_names and n + ".zip" in listing]
        # the set's own zips are tried one after the other (the parent's one is a fallback); a zip named for another
        # region is that region, and is checked on its own
        groups = ([main_paths] if main_paths else []) + [[path] for path in region_paths]

        if not groups:
            results["absent"] += 1
            if len(names) <= 2:
                what = "(no " + " / ".join(n + ".zip" for n in names) + " here)"
            else:
                what = "(no zip of this game here: " + ", ".join(names) + ")"
            print(f"{colors.paint(colors.dim, '[ -- ]')} {set_id}  {spec['name']}  {colors.paint(colors.dim, what)}", file=out)
            continue

        for candidates in groups:
            is_main = candidates[0] in main_paths
            outcomes = []
            for path in candidates:
                problems, used, extras, region = check_candidate(set_id, spec, path, listing, cache, quick)
                outcomes.append((path, problems, used, extras, region))
                if not problems:
                    break

            good = next((o for o in outcomes if not o[1]), None)
            if good:
                results["ok"] += 1
                region = good[4]
                zips = list(dict.fromkeys(z for _, z, _ in good[2]))  # in order of first use
                print(f"{colors.paint(colors.ok, '[ OK ]')} {region[0]}  {region[1]}  [{len(good[2])} files from {' + '.join(zips)}]", file=out)
                if verbose:
                    for name, zip_name, inner in good[2]:
                        renamed = f"  (stored as {inner})" if inner.lower() != name.lower() else ""
                        print(f"         {name:<12} {zip_name}{renamed}", file=out)
                wanted = {crc for _, _, crc in region[2]}
                for zip_name in zips:
                    needed.setdefault(listing[zip_name.lower()], set()).update(wanted)
                if good[3]:
                    results["extra"] += 1
                    primaries[good[0]] = good[3]
                    print_extras(os.path.basename(good[0]), good[3], colors, out)
            else:
                results["fail"] += 1
                stem = os.path.splitext(os.path.basename(candidates[0]))[0].lower()
                title = next(((rid, rname) for rid, rname, _ in regions(set_id, spec) if not is_main and rid.lower() == stem),
                             (set_id, spec["name"]))
                print(f"{colors.paint(colors.fail, '[FAIL]')} {title[0]}  {title[1]}", file=out)
                for hint in missing_zip_hints(set_id, spec, listing, candidates, is_main):
                    print(colors.paint(colors.warn, f"         ! {hint}"), file=out)
                for path, problems, _, extras, _ in outcomes:
                    print(f"         {os.path.basename(path)}:", file=out)
                    for problem in problems:
                        print(f"           - {problem}", file=out)
                    if extras:
                        print_extras(os.path.basename(path), extras, colors, out)

    # zips that no set uses by name: maybe a renamed set
    notes = []
    for lower, path in listing.items():
        if lower in names_in_use:
            continue
        look = find_lookalike(path, cache)
        if look:
            notes.append(f"{os.path.basename(path)} holds {look[1]}/{look[2]} files of {look[0]}; "
                         f"the launcher looks for it as {look[0]}.zip")
        elif verbose:
            notes.append(f"{os.path.basename(path)}: not used by cps2recomp (ignored)")
    for note in notes:
        print(f"{colors.paint(colors.warn, '[NOTE]')} {note}", file=out)

    if clean != "no" and primaries:
        for arc in cache.values():  # the zips are rewritten below, so they must not stay open
            if isinstance(arc, Archive):
                arc.close()
        cache.clear()
        for path, extras in primaries.items():
            keep = needed.get(path, set())
            remove = [e for e in extras if e[2] not in keep]
            name = os.path.basename(path)
            if not remove:
                continue
            if clean == "ask":
                note = "" if len(remove) == len(extras) else " (files that another set still uses are kept)"
                try:
                    answer = ask(f"\nRemove the {len(remove)} extra file(s) from {name}{note}? "
                                 "This rewrites the zip and cannot be undone. [y/N] ")
                except (EOFError, KeyboardInterrupt):
                    answer = ""
                if answer.strip().lower() not in ("y", "yes", "s", "sim"):
                    print(f"{colors.paint(colors.dim, '[SKIP]')} {name}: left unchanged", file=out)
                    continue
            try:
                count = rewrite_without_extras(path, {e[2] for e in remove}, backup)
                results["cleaned"] += 1
                print(f"{colors.paint(colors.ok, '[CLEAN]')} {name}: removed {count} extra file(s)", file=out)
            except (ValueError, zipfile.BadZipFile, OSError) as exc:
                print(f"{colors.paint(colors.fail, '[FAIL]')} {name}: not cleaned ({exc})", file=out)

    print(f"\n{results['ok']} valid, {results['fail']} invalid, {results['absent']} not present"
          f"{', ' + str(results['extra']) + ' with extra files' if results['extra'] else ''}"
          f"{', ' + str(results['cleaned']) + ' zip(s) cleaned' if results['cleaned'] else ''}"
          f"{'  (quick mode: file contents were not unpacked)' if quick else ''}", file=out)
    if results["ok"] == 0 and results["fail"] == 0:
        return 2
    return 1 if results["fail"] else 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="Validate ROM zips for cps2recomp (checksums as the launcher reads them).")
    parser.add_argument("-d", "--dir", default=".", help="folder with the zips (default: current folder)")
    parser.add_argument("-q", "--quick", action="store_true", help="trust the CRC32 stored in each zip instead of unpacking every file")
    parser.add_argument("-v", "--verbose", action="store_true", help="list every file of every valid set")
    parser.add_argument("--no-color", action="store_true", help="plain output")
    parser.add_argument("--list", action="store_true", help="list the supported sets and exit")
    parser.add_argument("--no-pause", action="store_true", help="do not wait for Enter before exiting")
    parser.add_argument("--no-clean", action="store_true", help="only report extra files, never offer to remove them")
    parser.add_argument("-y", "--yes", action="store_true", help="remove extra files from every zip without asking")
    parser.add_argument("--backup", action="store_true", help="keep <name>.zip.bak when a zip is rewritten")
    args = parser.parse_args(argv)

    try:
        return run(args)
    finally:
        if not args.no_pause:
            pause()


def pause():
    """Keeps a double-clicked console window open; does nothing when input or output is redirected."""
    if sys.stdin is None or not sys.stdin.isatty() or not sys.stdout.isatty():
        return
    try:
        input("\nPress Enter to exit...")
    except (EOFError, KeyboardInterrupt):
        pass


def run(args):
    if args.list:
        for set_id, spec in SETS.items():
            parent = spec["parent"] if spec["parent"] and spec["parent"] != set_id else "-"
            print(f"{set_id:<10} parent: {parent:<6} {len(spec['files']):>2} files  {spec['name']}")
            for other in spec.get("others", []):
                print(f"  + {other['id']:<10} other region of {set_id}: {other['name']}")
        return 0

    if not os.path.isdir(args.dir):
        print(f"Not a folder: {args.dir}", file=sys.stderr)
        return 2

    use_color = sys.stdout.isatty() and not args.no_color and "NO_COLOR" not in os.environ
    if use_color and os.name == "nt":
        os.system("")  # turns on ANSI escape handling in the Windows console
    interactive = sys.stdin is not None and sys.stdin.isatty() and sys.stdout.isatty()
    clean = "no" if args.no_clean else "yes" if args.yes else "ask" if interactive else "no"
    return validate(os.path.abspath(args.dir), args.quick, args.verbose, Colors(use_color), clean=clean, backup=args.backup)


if __name__ == "__main__":
    sys.exit(main())
