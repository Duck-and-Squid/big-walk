# UCLA Knowledge

## Build

```sh
clang++ -O3 -fopenmp -o walk walk.cpp -Iinclude
```

## Pipeline

```sh
./buildings.py >buildings.csv 2>buildings.log
./walk-matrix.py -i buildings.csv -o matrix.json
# below is by duration; or by distance
./walk -i matrix.json -o order-duration.json
./geojson.py -c buildings.csv -r order-duration.json
```
