# Data and Method

Where the data in `resources/` comes from and how it is built. For using the library, see the [usage guide](usage.md).

[TOC]

## Tiers

| Tier | Cities | Which |
| --- | --- | --- |
| `lite` | 33,521 | GeoNames `cities15000`: populated places with at least 15,000 people (or capitals) |
| `full` | 231,224 | GeoNames `cities500`: populated places with at least 500 people |

`resources/admin1CodesASCII.txt` holds the names of first-level divisions (states, provinces) for `resolve_admin1()`. It is loaded separately with `load_admin1()` and is not part of a tier.

## Source

All data comes from [GeoNames](https://www.geonames.org/) exports. `scripts/prepare_geonames.py` downloads `cities15000` and `cities500`, keeps 16 of their 19 columns (it drops `alternatenames`, `feature_class` and `modification_date`) and writes the two tiers:

```bash
python scripts/prepare_geonames.py
```

After changing the data, run `make embed` to regenerate the compiled-in copy, and update the known-answer tests (the data hash and results change).

## Columns

| Column | Meaning | Notes |
| --- | --- | --- |
| `geonameid` | GeoNames id | Key of extension layers; must be positive |
| `name`, `asciiname` | Name in UTF-8; ASCII transliteration | |
| `latitude`, `longitude` | WGS84 decimal degrees | |
| `feature_code` | GeoNames feature code | `PPL` populated place, `PPLA` division seat, `PPLC` capital, ... |
| `country_code`, `cc2` | ISO country code; alternate codes | `country_code` drives `get_city(country)`; `cc2` is nearly always empty |
| `admin1_code` … `admin4_code` | Administrative division codes | Often empty below level 1 |
| `population` | Population | Selection weight; 0 counts as 1, so every city can be drawn (1 city in `lite`, 30,581 in `full` have 0) |
| `elevation` | Surveyed elevation in metres | Usually empty: unknown is -9999 (null in JSON) |
| `dem` | Elevation from a digital elevation model, in metres | -9999 over sea |
| `timezone` | IANA time zone | |

## Generation Model

- **Weighted (default):** a city's chance is its population divided by the total population loaded (or of its country, for `get_city(country)`).
- **Uniform:** every loaded city (or every city in the country) is equally likely.

Both draw from the `"city"` stream split from the call seed, with dasmig-core's weighted choice and uniform integer, so the result is the same on every platform.

## Known Limitations

- GeoNames populations come from different years and sources per country, and many small places have none (they count as 1).
- Admin1 names are not part of the data hash or of extension layers; they do not affect which city is drawn.
- The full tier takes about a second to load (231,224 rows, each checked); the `lite` tier about 0.2 s.
