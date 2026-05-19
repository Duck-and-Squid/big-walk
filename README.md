# UCLA Knowledge

## Build

```sh
clang++ -o walk walk.cpp -Iinclude
```

## Pipeline

```sh
./buildings.py >buildings.csv 2>buildings.log
./walk-matrix.py -i buildings.csv -o matrix.json
./walk -i matrix.json -o order-distance.json -m distance --open
./walk -i matrix.json -o order-duration.json -m duration --open
./geojson.py -c buildings.csv -r order-duration.json # or by distance
```
