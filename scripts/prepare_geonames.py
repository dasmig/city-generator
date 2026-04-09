#!/usr/bin/env python3
"""Download GeoNames data and extract clean TSV files for city-generator.

Produces two tiers mirroring the name-generator pattern:
  resources/full/cities.tsv  — cities500 (~200k cities, pop ≥ 500)
  resources/lite/cities.tsv  — cities15000 (~25k major cities, pop ≥ 15000)

Drops: alternatenames (field 3), feature_class (field 6), modification_date (field 18).
Keeps the remaining 16 fields in order:
  geonameid, name, asciiname, latitude, longitude, feature_code,
  country_code, cc2, admin1_code, admin2_code, admin3_code, admin4_code,
  population, elevation, dem, timezone

Usage:
    python prepare_geonames.py              # generate both tiers
    python prepare_geonames.py --tier full  # full only
    python prepare_geonames.py --tier lite  # lite only
"""

import argparse
import io
import os
import urllib.request
import zipfile

# GeoNames field indices (0-based) in the raw tab-delimited file.
# See: https://download.geonames.org/export/dump/readme.txt
_KEEP_INDICES = [
    0,   # geonameid
    1,   # name (UTF-8)
    2,   # asciiname
    4,   # latitude
    5,   # longitude
    7,   # feature_code
    8,   # country_code
    9,   # cc2
    10,  # admin1_code
    11,  # admin2_code
    12,  # admin3_code
    13,  # admin4_code
    14,  # population
    15,  # elevation
    16,  # dem
    17,  # timezone
]

_HEADER = "\t".join([
    "geonameid", "name", "asciiname", "latitude", "longitude",
    "feature_code", "country_code", "cc2",
    "admin1_code", "admin2_code", "admin3_code", "admin4_code",
    "population", "elevation", "dem", "timezone",
])

_BASE_URL = "https://download.geonames.org/export/dump/"


def download_and_extract(source: str, output: str) -> None:
    zip_name = f"{source}.zip"
    txt_name = f"{source}.txt"
    url = _BASE_URL + zip_name

    print(f"Downloading {url} ...")
    resp = urllib.request.urlopen(url)  # noqa: S310 — trusted URL
    data = resp.read()
    print(f"Downloaded {len(data)} bytes.")

    print(f"Extracting {txt_name} ...")
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        raw = zf.read(txt_name).decode("utf-8")

    lines_in = raw.splitlines()
    lines_out: list[str] = [_HEADER]

    for line in lines_in:
        fields = line.split("\t")
        if len(fields) < 19:
            continue
        kept = [fields[i] for i in _KEEP_INDICES]
        lines_out.append("\t".join(kept))

    os.makedirs(os.path.dirname(output) or ".", exist_ok=True)
    with open(output, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines_out))
        f.write("\n")

    # lines_out includes the header row
    print(f"Wrote {len(lines_out) - 1} cities to {output}")


_TIERS = {
    "full": "cities500",
    "lite": "cities15000",
}


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Prepare GeoNames TSV files for city-generator."
    )
    parser.add_argument(
        "--tier",
        default="both",
        choices=["full", "lite", "both"],
        help="Which dataset tier to generate (default: both).",
    )
    args = parser.parse_args()

    base_dir = os.path.join(os.path.dirname(__file__), "..", "resources")
    tiers = list(_TIERS.keys()) if args.tier == "both" else [args.tier]

    for tier in tiers:
        source = _TIERS[tier]
        output = os.path.join(base_dir, tier, "cities.tsv")
        print(f"\n=== {tier} ({source}) ===")
        download_and_extract(source, output)


if __name__ == "__main__":
    main()
