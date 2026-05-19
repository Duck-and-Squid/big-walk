# Big Walk

Location extraction and route planning for the big walk. Largely vibe-coded.

## Setup

Assuming you have Homebrew installed.

Tested with Homebrew clang version 22.1.4:

```sh
brew install llvm libomp
# Verify this is using homebrew; should be something like:
#   /opt/homebrew/opt/llvm/bin/clang++
which clang++
# You may need to follow some additional instructions to setup Homebrew LLVM.
# Be sure to also set OpenMP_ROOT accordingly for this to compile.
clang++ -O3 -fopenmp -o walk walk.cpp -Iinclude
```

Tested with Python 3.14.2:

```sh
# Optionally, create a virtual environment.
python3.14 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Pipeline

```sh
./buildings.py >buildings.csv 2>buildings.log
./walk-matrix.py -i buildings.csv -o matrix.json
# below is by duration; or by distance
./walk -i matrix.json -o order-duration.json
./geojson.py -c buildings.csv -r order-duration.json
```
