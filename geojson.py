#!/usr/bin/env python3

import argparse
import csv
import json
import sys


def parse_args():
    parser = argparse.ArgumentParser(
        description="Generate a GeoJSON FeatureCollection from optimized route data."
    )
    parser.add_argument(
        "-c",
        "--csv",
        required=True,
        help="Path to the original input CSV file containing coordinates.",
    )
    parser.add_argument(
        "-r",
        "--route",
        required=True,
        help="Path to the optimized route JSON file.",
    )
    parser.add_argument(
        "-o",
        "--output",
        help="Optional: Path to save the output as a .geojson file.",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    # Load original coordinates from CSV
    all_coords = []
    try:
        with open(args.csv, newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                all_coords.append([float(row["lng"]), float(row["lat"])])
    except Exception as e:
        print(f"Error reading CSV: {e}", file=sys.stderr)
        sys.exit(1)

    # Load optimized indices
    try:
        with open(args.route, "r", encoding="utf-8") as f:
            route_data = json.load(f)
    except Exception as e:
        print(f"Error reading JSON: {e}", file=sys.stderr)
        sys.exit(1)

    indices = route_data.get("optimal_order_indices", [])
    is_round_trip = route_data.get("route_type") == "round-trip"

    if not indices:
        print(
            "Error: 'optimal_order_indices' missing from route file.", file=sys.stderr
        )
        sys.exit(1)

    # Map indices to the [Lng, Lat] pairs
    ordered_stops = [all_coords[idx] for idx in indices]

    # If it's a loop, add the starting point back to the end to close the shape
    if is_round_trip:
        ordered_stops.append(ordered_stops[0])

    start_point = ordered_stops[0]
    end_point = ordered_stops[-1]

    # Build features list starting with the path itself
    features = [
        {
            "type": "Feature",
            "properties": {
                "name": "Optimized TSP Route",
                "stroke": "#3388ff",
                "stroke-width": 4,
            },
            "geometry": {"type": "LineString", "coordinates": ordered_stops},
        },
        {
            "type": "Feature",
            "properties": {
                "name": "Start/End Point" if is_round_trip else "Start Point",
                "marker-color": "#00aa00",
                "marker-symbol": "star",
            },
            "geometry": {"type": "Point", "coordinates": start_point},
        },
    ]

    # If it's a one-way trip, add a distinct end point marker
    if not is_round_trip:
        features.append(
            {
                "type": "Feature",
                "properties": {
                    "name": "End Point",
                    "marker-color": "#aa0000",
                    "marker-symbol": "square",
                },
                "geometry": {"type": "Point", "coordinates": end_point},
            }
        )

    # Build the standard GeoJSON FeatureCollection structure
    geojson_data = {"type": "FeatureCollection", "features": features}

    # Convert the dictionary to a formatted JSON string
    geojson_string = json.dumps(geojson_data, indent=2)

    # Either save to a file or print directly to the console for stdout piping
    if args.output:
        try:
            with open(args.output, "w", encoding="utf-8") as f:
                f.write(geojson_string)
            print(f"Success! GeoJSON saved to {args.output}", file=sys.stderr)
        except Exception as e:
            print(f"Failed to save file: {e}", file=sys.stderr)
    else:
        # Standard output solely reserved for the content
        print(geojson_string)


if __name__ == "__main__":
    main()
