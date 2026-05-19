#!/usr/bin/env python3
import argparse
import csv
import json
import sys


def main():
    parser = argparse.ArgumentParser(
        description="Prepare sorted CSV for Notion import."
    )
    parser.add_argument(
        "-c", "--csv", required=True, help="Path to input buildings.csv"
    )
    parser.add_argument("-j", "--json", required=True, help="Path to input order.json")
    parser.add_argument(
        "-o", "--output", required=True, help="Path to output sorted CSV"
    )
    args = parser.parse_args()

    # Load the optimized order indices
    try:
        with open(args.json, "r") as f:
            route_data = json.load(f)
        indices = route_data["optimal_order_indices"]
    except Exception as e:
        print(f"ERROR: Failed to read JSON: {e}", file=sys.stderr)
        sys.exit(1)

    # Load the original CSV rows
    try:
        with open(args.csv, "r", newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            fieldnames = reader.fieldnames
            rows = list(reader)
    except Exception as e:
        print(f"ERROR: Failed to read CSV: {e}", file=sys.stderr)
        sys.exit(1)

    if not fieldnames:
        print("ERROR: Input CSV has no headers.", file=sys.stderr)
        sys.exit(1)

    # Validate indices bounds against CSV row count
    if max(indices) >= len(rows):
        print(
            "ERROR: JSON indices do not match the number of rows in the CSV.",
            file=sys.stderr,
        )
        sys.exit(1)

    # Reorder rows and inject explicit step number
    sorted_rows = []
    for step_num, idx in enumerate(indices, start=1):
        row = rows[idx].copy()
        row["Route Order"] = step_num
        sorted_rows.append(row)

    # Write output CSV with the new column first
    output_fieldnames = ["Route Order"] + [f for f in fieldnames if f != "Route Order"]

    try:
        with open(args.output, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=output_fieldnames)
            writer.writeheader()
            writer.writerows(sorted_rows)
        print(f"INFO: Successfully saved Notion-ready CSV to {args.output}")
    except Exception as e:
        print(f"ERROR: Failed to write output CSV: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
