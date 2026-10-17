#!/usr/bin/env python3
"""The games the toolkit knows: one entry per game, chosen by the TITLE_ID in sce_sys/param.sfo.

Everything the scripts need to know about one game lives here: its serials, the update it runs
with (hooks and patches use the addresses of that one executable), the hash of that executable's
loaded image, where its data is and how a damaged extraction shows in it, and its built-in patch
file. The other scripts ask for the game of a folder (game_for) instead of naming one.
"""
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


@dataclass(frozen=True)
class Game:
    key: str
    name: str
    serials: tuple
    version: str  # APP_VER of the update the toolkit runs
    image: str  # SHA-256 of that update's loaded eboot.bin image (game_check.image_sha256)
    data: str  # 'psarc': archives at the root of the game folder
    data_root: str = ''  # relative to the game folder
    other_images: dict = field(default_factory=dict)  # known bad images: hash -> problem kind
    base_image: str = ''  # the base game's image (1.00): the update is missing


WIPEOUT = Game(
    key='wipeout',
    name='WipEout Omega Collection',
    # The European release; other regions are added once their 1.07 image is known to match.
    serials=('CUSA05670',),
    version='01.07',
    image='5248bcb91055013f7a81743a4b2fd34ac6c8968045deb47ae13b2ab7b8439920',
    base_image='33d92be2d82c991a7d4eb41c19d289c0dca24b45ba821f503cafef40ae5b196d',
    data='psarc',
)

# The target of this fork first: a folder without a readable param.sfo is taken for it.
GAMES = (WIPEOUT,)
DEFAULT = WIPEOUT


def title_info(game_dir):
    """(TITLE_ID, APP_VER) from param.sfo, '?' for what cannot be read."""
    from prepare import sfo  # same directory
    try:
        info = sfo((Path(game_dir) / 'sce_sys/param.sfo').read_bytes())
    except (OSError, ValueError, IndexError):
        info = {}
    return info.get('TITLE_ID', '?'), info.get('APP_VER', '?')


def by_serial(serial):
    return next((g for g in GAMES if serial in g.serials), None)


def game_for(game_dir):
    """The known game of a folder, or DEFAULT when its serial is unknown or unreadable."""
    return by_serial(title_info(game_dir)[0]) or DEFAULT


def data_files(game_dir, game=None):
    """The game's data files (for statistics and the damage check)."""
    game = game or game_for(game_dir)
    root = Path(game_dir) / game.data_root
    return sorted(p for p in root.glob('*.psarc') if p.is_file())
