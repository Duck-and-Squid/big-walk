#!/usr/bin/env python3

import argparse
import csv
import itertools
import json
import logging
import os
import time

import openrouteservice
from dotenv import load_dotenv

# Setup basic logging instead of using print()
logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compute a distance matrix using OpenRouteService with safe sub-matrix chunking and disk caching."
    )
    parser.add_argument(
        "-i",
        "--input",
        required=True,
        help="Path to the input CSV file containing 'name', 'lng', and 'lat' columns.",
    )
    parser.add_argument(
        "-o", "--output", required=True, help="Path to the output JSON file."
    )
    parser.add_argument(
        "-c",
        "--chunk-size",
        type=int,
        default=35,
        help="Max number of locations per chunk (Sub-matrix limits). Default: 35",
    )
    parser.add_argument(
        "--sleep",
        type=float,
        default=2.0,
        help="Sleep duration between API calls to avoid rate limits. Default: 2.0s",
    )
    parser.add_argument(
        "--cache-dir",
        default="ors_cache",
        help="Directory to save individual chunk responses. Default: ors_cache",
    )
    return parser.parse_args()


def load_points(filepath: str) -> tuple[list[str], list[list[float]]]:
    """Loads points from a CSV file and returns names and coordinates."""
    names = []
    coords = []
    try:
        with open(filepath, newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row_num, row in enumerate(reader, start=1):
                if "name" not in row or "lng" not in row or "lat" not in row:
                    raise ValueError(f"Missing required columns in row {row_num}")
                names.append(row["name"])
                coords.append([float(row["lng"]), float(row["lat"])])
    except FileNotFoundError:
        logging.error(f"Input file not found: {filepath}")
        raise
    except Exception as e:
        logging.error(f"Error reading {filepath}: {e}")
        raise

    return names, coords


def get_chunked_matrix(
    client: openrouteservice.Client,
    coords: list[list[float]],
    chunk_size: int,
    sleep_time: float,
    cache_dir: str,
) -> tuple[list[list[float]], list[list[float]]]:
    """
    Safely fetches the distance and duration matrix by breaking coordinates
    into smaller sub-matrices, caching chunks to disk, and stitching them together.
    """
    n = len(coords)

    # Initialize empty N x N matrices
    distances = [[0.0] * n for _ in range(n)]
    durations = [[0.0] * n for _ in range(n)]

    # Create sequential chunks of indices
    indices = list(range(n))
    chunks = [indices[i : i + chunk_size] for i in range(0, n, chunk_size)]
    
    # Ensure cache directory exists
    os.makedirs(cache_dir, exist_ok=True)

    # Pair chunk indices so we can name cache files deterministically
    enumerated_chunks = list(enumerate(chunks))
    total_batches = len(chunks) ** 2
    logging.info(f"Splitting matrix into {total_batches} sub-requests...")

    batch_num = 1
    for (r_idx, row_chunk), (c_idx, col_chunk) in itertools.product(enumerated_chunks, repeat=2):
        cache_filename = f"chunk_r{r_idx}_c{c_idx}.json"
        cache_filepath = os.path.join(cache_dir, cache_filename)

        # Check if this specific sub-matrix chunk is already cached on disk
        if os.path.exists(cache_filepath):
            logging.info(f"[{batch_num}/{total_batches}] Loading cached grid element: {cache_filename}")
            with open(cache_filepath, "r", encoding="utf-8") as f:
                result = json.load(f)
            from_cache = True
        else:
            logging.info(f"[{batch_num}/{total_batches}] Cache miss. Fetching API data for: {cache_filename}")
            
            unique_indices = list(dict.fromkeys(row_chunk + col_chunk))
            req_coords = [coords[i] for i in unique_indices]

            source_idxs = [unique_indices.index(i) for i in row_chunk]
            dest_idxs = [unique_indices.index(i) for i in col_chunk]

            try:
                result = client.distance_matrix(
                    locations=req_coords,
                    sources=source_idxs,
                    destinations=dest_idxs,
                    profile="foot-walking",
                    metrics=["distance", "duration"],
                )
                
                # Immediately write the successful response payload to disk
                with open(cache_filepath, "w", encoding="utf-8") as f:
                    json.dump(result, f, indent=2)
                from_cache = False

            except openrouteservice.exceptions.ApiError as e:
                logging.error(f"API Error during batch {batch_num}: {e}")
                raise

        # Populate the master matrices with the sub-matrix results (Cached or Live)
        for i, row_idx in enumerate(row_chunk):
            for j, col_idx in enumerate(col_chunk):
                distances[row_idx][col_idx] = result["distances"][i][j]
                durations[row_idx][col_idx] = result["durations"][i][j]

        # Only sleep if we performed an actual network call and it's not the final batch
        if not from_cache and batch_num < total_batches:
            time.sleep(sleep_time)
            
        batch_num += 1

    return distances, durations


def save_output(filepath: str, data: dict):
    """Saves the final dictionary to a JSON file."""
    try:
        with open(filepath, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        logging.info(f"Successfully saved consolidated results to {filepath}")
    except Exception as e:
        logging.error(f"Failed to write output to {filepath}: {e}")
        raise


def main():
    args = parse_args()

    # Environment Check
    load_dotenv()
    api_key = os.environ.get("OPENROUTESERVICE_API_KEY")
    if not api_key:
        logging.error(
            "No OPENROUTESERVICE_API_KEY environment variable set in .env or OS."
        )
        return

    # Initialization
    client = openrouteservice.Client(key=api_key)

    # Data Loading
    logging.info(f"Loading coordinates from {args.input}...")
    names, coords = load_points(args.input)

    if not coords:
        logging.warning("No coordinates found in the input file.")
        return

    logging.info(f"Loaded {len(coords)} points.")

    # Fetch Data (with caching lookup enabled)
    distances, durations = get_chunked_matrix(
        client=client, 
        coords=coords, 
        chunk_size=args.chunk_size, 
        sleep_time=args.sleep,
        cache_dir=args.cache_dir
    )

    # Save Output
    output = {"names": names, "distances_m": distances, "durations_s": durations}
    save_output(args.output, output)


if __name__ == "__main__":
    main()