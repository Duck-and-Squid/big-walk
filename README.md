# UCLA Knowledge

## Pipeline

```sh
./buildings.py >buildings.csv 2>buildings.log
./walk-matrix.py -i buildings.csv -o matrix.json
./walk.py -i matrix.json -o order-distance.json -m distance --open
./walk.py -i matrix.json -o order-duration.json -m duration --open
```
