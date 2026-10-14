# City Generator for C++

> **Requires C++23** (e.g., `-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

[![City Generator for C++](https://raw.githubusercontent.com/dasmig/city-generator/master/doc/city-generator.png)](https://github.com/dasmig/city-generator/releases)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/city-generator/master/LICENSE.MIT)
[![CI](https://github.com/dasmig/city-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/dasmig/city-generator/actions/workflows/ci.yml)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/city-generator.svg)](https://github.com/dasmig/city-generator/releases)
[![GitHub Issues](https://img.shields.io/github/issues/dasmig/city-generator.svg)](https://github.com/dasmig/city-generator/issues)
[![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/type-header--only-green.svg)](https://github.com/dasmig/city-generator#integration)
[![Platform](https://img.shields.io/badge/platform-linux%20|%20windows%20|%20macos-lightgrey.svg)](https://github.com/dasmig/city-generator)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue.svg)](https://dasmig.github.io/city-generator/)

**[API Reference](https://dasmig.github.io/city-generator/)** · **[Usage Guide](doc/usage.md)** · **[Data and Method](doc/data.md)** · **[Releases](https://github.com/dasmig/city-generator/releases)**

## Features

- **Population-Weighted Generation**. Cities are selected with probability proportional to their population, mirroring real-world demographic distributions.

- **Rich City Data**. Every generated city includes 16 GeoNames fields: name, coordinates, country, administrative divisions, population, elevation, timezone, and more.

- **Country Filtering**. Generate cities from a specific country with `get_city({.country = "BR"})`.

- **GeoNames Data**. Ships with two dataset tiers — *full* (231,224 cities, pop ≥ 500) and *lite* (33,521 major cities, pop ≥ 15,000) — from GeoNames, licensed under CC BY 4.0.

- **Deterministic Seeding**. Per-call `get_city(request, seed)` for reproducible results, generator-level `seed()` / `unseed()` for deterministic sequences, and `city::seed()` for replaying a previous generation.

- **Request-First API**. `get_city(request)` and `get_city(request, seed)` take a `city_request` with an optional country filter and a population-weighted (default) or uniform selection mode. There are no generator-level defaults.

- **Identical Everywhere**. A seed gives the same city on every compiler, standard library and CPU (GCC, Clang, MSVC, Emscripten; x86-64 and ARM64), checked by known-answer tests.

- **Extensible Data**. Load your own TSV layers on top of the shipped data to add, change or remove cities (`extend()`), checked before they are applied.

- **JSON and Replay**. `to_json()` on every result, and a `signature()` recording the seed and the data it came from.

- **Compiled-in Data**. Define `DASMIG_CITY_EMBED` to build the `lite` tier into the program, for WebAssembly and other builds without files.

- **Multi-Instance Support**. Construct independent `cg` instances with their own data and random engine — ideal for embedding inside other generators.

- **Admin1 Code Resolution**. Translate raw admin1 codes (e.g. `"CA"`, `"08"`) into human-readable state/province names using the GeoNames `admin1CodesASCII.txt` mapping.

## Integration

[`citygen.hpp`](https://github.com/dasmig/city-generator/blob/master/dasmig/citygen.hpp) is the library [released here](https://github.com/dasmig/city-generator/releases). Keep [`core.hpp`](https://github.com/dasmig/city-generator/blob/master/dasmig/core.hpp) and the [`core/`](https://github.com/dasmig/city-generator/tree/master/dasmig/core) directory ([dasmig-core](https://github.com/dasmig/dasmig-core)) in the same directory, and add

```cpp
#include <dasmig/citygen.hpp>

// For convenience.
using cg = dasmig::cg;
```

to the files you want to generate cities and set the necessary switches to enable C++23 (e.g., `-std=c++23` for GCC and Clang).

Additionally you must supply the city generator with the [`resources`](https://github.com/dasmig/city-generator/tree/master/resources) folder containing `full/cities.tsv` and/or `lite/cities.tsv`, also available in the [release](https://github.com/dasmig/city-generator/releases).

## Build and Test

Run the following commands from the repository root with GNU Make, a Unix-like shell, and a C++23 compiler and standard library. The Makefile defaults to `clang++`; Linux CI uses LLVM 18 and libstdc++ 14.

```bash
make all        # build the example
./build/example # run the example
make test       # build and run the tests
make test-embed # build and run the tests with the lite tier compiled in
```

Binaries are written to `build/`. Keep the checked-in `resources/` folder available for the example and file-backed tests. To use a different compiler, override `CXX`, for example: `make test CXX=g++-14`.

To generate coverage reports with Clang, also install `llvm-profdata` and `llvm-cov`, then run:

```bash
make coverage
```

The HTML report is written to `coverage/index.html`.

## Usage

```cpp
#include <dasmig/citygen.hpp>
#include <iostream>

// For convenience.
using cg = dasmig::cg;

// Manually load a specific dataset tier if necessary.
cg::instance().load(dasmig::dataset::lite);  // 33,521 major cities
// OR
cg::instance().load(dasmig::dataset::full);  // 231,224 cities

// Generate a random city (population-weighted).
auto city = cg::instance().get_city();
std::cout << city.name << ", " << city.country_code
          << " (pop. " << city.population << ")" << std::endl;

// Generate a city from a specific country.
auto br_city = cg::instance().get_city({.country = "BR"});

// Access all available fields.
std::cout << city.latitude << ", " << city.longitude << std::endl;
std::cout << city.timezone << std::endl;
std::cout << city.elevation << " m" << std::endl;

// Deterministic generation — same request and seed always produce the same city.
auto seeded = cg::instance().get_city({}, 42);

// Replay a previous city using its seed.
auto replay = cg::instance().get_city({}, seeded.seed());

// Uniform selection within Brazil.
const dasmig::city_request request{.country = "BR", .weighted = false};
auto requested = cg::instance().get_city(request, 42);
auto requested_replay = cg::instance().get_city(request, requested.seed());

// A default request selects globally with population weighting.
// Replay requires the same request and loaded data as well as the seed.

// Seed the engine for a deterministic sequence.
cg::instance().seed(100);
// ... generate cities ...
cg::instance().unseed(); // restore non-deterministic state

// Uniform selection: any city equally likely.
auto uniform_city = cg::instance().get_city({.weighted = false});

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
| full | `cities500.zip` | 231,224 (pop ≥ 500) |
| lite | `cities15000.zip` | 33,521 (pop ≥ 15,000) |

The data is licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). See `LICENSE_DATA.txt` for details.

To regenerate datasets:

```bash
python scripts/prepare_geonames.py            # generate both tiers
python scripts/prepare_geonames.py --tier lite # lite only
python scripts/prepare_geonames.py --tier full # full only
```

## Related Libraries

| Library | Description |
|---------|-------------|
| [name-generator](https://github.com/dasmig/name-generator) | Culturally appropriate full names |
| [nickname-generator](https://github.com/dasmig/nickname-generator) | Gamer-style nicknames |
| [birth-generator](https://github.com/dasmig/birth-generator) | Demographically plausible birthdays |
| [biodata-generator](https://github.com/dasmig/biodata-generator) | Procedural human physical characteristics |
| [country-generator](https://github.com/dasmig/country-generator) | Weighted country selection by population |
| [entity-generator](https://github.com/dasmig/entity-generator) | ECS-based entity generation |
