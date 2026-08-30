#!/usr/bin/env python3
"""Télécharge les images de test Big Buck Bunny utilisées par ce laboratoire dans
data/downloaded/ (ou un autre dossier choisi avec -d/--dest)."""

import argparse
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

DEFAULT_DATA_DIR = Path(__file__).resolve().parent / "downloaded"
BASE_URL = "https://media.xiph.org/BBB/BBB-360-png/big_buck_bunny_{:05d}.png"
FRAME_OFFSET = 5580
FRAME_COUNT = 401
PARALLEL_DOWNLOADS = 16


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "count", nargs="?", default="tous",
        help=f"nombre d'images à télécharger (entre 1 et {FRAME_COUNT}), ou 'tous' pour les "
             "télécharger toutes (défaut: tous)",
    )
    parser.add_argument(
        "-d", "--dest", type=Path, default=DEFAULT_DATA_DIR,
        help=f"dossier où enregistrer les images téléchargées (défaut: {DEFAULT_DATA_DIR})",
    )
    args = parser.parse_args()

    if args.count == "tous":
        args.count = FRAME_COUNT
        return args

    try:
        count = int(args.count)
    except ValueError:
        parser.error(f"nombre d'images invalide : '{args.count}' (attendu un entier ou 'tous')")
    if not 1 <= count <= FRAME_COUNT:
        parser.error(f"nombre d'images doit être entre 1 et {FRAME_COUNT} (reçu {count})")
    args.count = count
    return args


def fetch_frame(local_index: int, dest_dir: Path) -> bool:
    url = BASE_URL.format(local_index + FRAME_OFFSET)
    dest = dest_dir / f"{local_index:04d}.png"
    try:
        urllib.request.urlretrieve(url, dest)
    except OSError as error:
        print(f"\n{dest.name}: {error}", file=sys.stderr)
        return False
    print(".", end="", flush=True)
    return True


def main() -> int:
    args = parse_args()
    args.dest.mkdir(parents=True, exist_ok=True)

    with ThreadPoolExecutor(max_workers=PARALLEL_DOWNLOADS) as pool:
        results = list(pool.map(lambda i: fetch_frame(i, args.dest), range(args.count)))
    print()
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
