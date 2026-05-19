#!/usr/bin/env python3

import csv
import json
import sys


RELEVANT = ("name", "lat", "lng", "region")

HILL_BBOX = (34.069156, -118.455362, 34.076905, -118.448861)
NORTH_BBOX = (34.073271, -118.444837, 34.077981, -118.438711)
CENTRAL_BBOX = (34.069831, -118.449558, 34.073751, -118.437778)
SOUTH_BBOX = (34.066472, -118.448818, 34.069823, -118.439333)
DEEP_SOUTH_BBOX = (34.063770, -118.445191, 34.067050, -118.440031)

REGIONS = (
    ("Hill", HILL_BBOX),
    ("North", NORTH_BBOX),
    ("Central", CENTRAL_BBOX),
    ("South", SOUTH_BBOX),
    ("Deep South", DEEP_SOUTH_BBOX),
)

BLACKLIST = (
    "Krieger",
    "Lab School",
    "Fernald",
    "Pool",
    "Sproul",
    "De Neve",
    "Dykstra",
    "Olympic",
    "Centennial",
    "Rieber",
    "Delta Terrace",
    "Canyon Point",
    "Courtside",
    "Hedrick",
    "Hitch",
    "Saxon",
    "Apartments",
    "Facility",
    "Facilities",
    "DWP",
    "ATS Modular Data Center 1",
    "Bus Terminal, Hilgard",
    "Police Station",
    "PS 27 Generator Building",
    "Marion Davies Children's Health Center"
)


def in_bbox(lat, lng, bbox):
    min_lat, min_lng, max_lat, max_lng = bbox
    return min_lat <= lat <= max_lat and min_lng <= lng <= max_lng


def get_region(lat, lng):
    for name, bbox in REGIONS:
        if in_bbox(lat, lng, bbox):
            return name
    return None


def is_acceptable(row):
    name = row["name"]
    for blacklisted in BLACKLIST:
        if blacklisted in name:
            print(f"BLACKLIST\t{name}", file=sys.stderr)
            return False
    if row["region"] is None:
        print(f"OUTSIDE\t{name}", file=sys.stderr)
        return False
    return True


def get_building_rows(data):
    rows = data[0]["children"]["locations"]

    result = []
    for row in rows:
        lat = float(row["lat"])
        lng = float(row["lng"])
        region = get_region(lat, lng)
        row = {
            "name": row["name"],
            "lat": lat,
            "lng": lng,
            "region": region,
        }
        if is_acceptable(row):
            result.append(row)

    return result


if __name__ == "__main__":
    with open("raw/buildings.json") as f:
        data = json.load(f)

    rows = get_building_rows(data)
    w = csv.DictWriter(sys.stdout, fieldnames=RELEVANT)
    w.writeheader()
    for row in sorted(rows, key=lambda r: (r["region"], r["name"])):
        w.writerow(row)
