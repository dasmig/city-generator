# City Generator for C++

> **Requires C++23** (e.g., `-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/city-generator/master/LICENSE.MIT)
[![CI](https://github.com/dasmig/city-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/dasmig/city-generator/actions/workflows/ci.yml)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/city-generator.svg)](https://github.com/dasmig/city-generator/releases)
[![GitHub Issues](https://img.shields.io/github/issues/dasmig/city-generator.svg)](https://github.com/dasmig/city-generator/issues)
[![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/type-header--only-green.svg)](https://github.com/dasmig/city-generator#integration)
[![Platform](https://img.shields.io/badge/platform-linux%20|%20windows%20|%20macos-lightgrey.svg)](https://github.com/dasmig/city-generator)

**[Usage Guide](doc/usage.md)** · **[Releases](https://github.com/dasmig/city-generator/releases)**

## Features

- **Population-Weighted Generation**. Cities are selected with probability proportional to their population, mirroring real-world demographic distributions.

- **Rich City Data**. Every generated city includes 16 GeoNames fields: name, coordinates, country, administrative divisions, population, elevation, timezone, and more.

- **Country Filtering**. Generate cities from a specific country with `get_city("BR")`.

- **GeoNames Data**. Ships with two dataset tiers — *full* (~200k cities, pop ≥ 500) and *lite* (~25k major cities, pop ≥ 15,000) — from GeoNames, licensed under CC BY 4.0.

- **Deterministic Seeding**. Per-call `get_city(seed)` for reproducible results, generator-level `seed()` / `unseed()` for deterministic sequences, and `city::seed()` for replaying a previous generation.

- **Multi-Instance Support**. Construct independent `cg` instances with their own data and random engine — ideal for embedding inside other generators.

## Integration

[`citygen.hpp`](https://github.com/dasmig/city-generator/blob/master/dasmig/citygen.hpp) is the single required file [released here](https://github.com/dasmig/city-generator/releases). You need to add

```cpp
#include <dasmig/citygen.hpp>

// For convenience.
using cg = dasmig::cg;
```

to the files you want to generate cities and set the necessary switches to enable C++23 (e.g., `-std=c++23` for GCC and Clang).

Additionally you must supply the city generator with the [`resources`](https://github.com/dasmig/city-generator/tree/master/resources) folder containing `full/cities.tsv` and/or `lite/cities.tsv`, also available in the [release](https://github.com/dasmig/city-generator/releases).

## Usage

```cpp
#include <dasmig/citygen.hpp>
#include <iostream>

// For convenience.
using cg = dasmig::cg;

// Manually load a specific dataset tier if necessary.
cg::instance().load(dasmig::dataset::lite);  // ~25k major cities
// OR
cg::instance().load(dasmig::dataset::full);  // ~200k cities

// Generate a random city (population-weighted).
auto city = cg::instance().get_city();
std::cout << city.name << ", " << city.country_code
          << " (pop. " << city.population << ")" << std::endl;

// Generate a city from a specific country.
auto br_city = cg::instance().get_city("BR");

// Access all available fields.
std::cout << city.latitude << ", " << city.longitude << std::endl;
std::cout << city.timezone << std::endl;
std::cout << city.elevation << " m" << std::endl;

// Deterministic generation — same seed always produces the same city.
auto seeded = cg::instance().get_city(42);

// Replay a previous city using its seed.
auto replay = cg::instance().get_city(seeded.seed());

// Seed the engine for a deterministic sequence.
cg::instance().seed(100);
// ... generate cities ...
cg::instance().unseed(); // restore non-deterministic state

// Independent instance — separate data and random engine.
cg my_gen;
my_gen.load("path/to/cities.tsv");
auto c = my_gen.get_city();
```

For the complete feature guide — fields, filtering, weighting, and more — see the **[Usage Guide](doc/usage.md)**.

## Data

The city data is sourced from the [GeoNames](https://www.geonames.org/) geographical database. Two tiers are provided:

| Tier | Source | Cities |
|------|--------|--------|
| full | `cities500.zip` | ~200k (pop ≥ 500) |
| lite | `cities15000.zip` | ~25k (pop ≥ 15,000) |

The data is licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). See `LICENSE_DATA.txt` for details.

To regenerate datasets:

```bash
python scripts/prepare_geonames.py            # generate both tiers
python scripts/prepare_geonames.py --tier lite # lite only
python scripts/prepare_geonames.py --tier full # full only
```
