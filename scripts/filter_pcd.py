import re
import argparse

def read_ascii_pcd(file_path, tag_field, tag_value):
    """
    Reads an ASCII PCD file, filters points based on a tag field and value, 
    and calculates the min and max Y values for the filtered points.

    Args:
        file_path (str): Path to the PCD file.
        tag_field (str): The field name of the tag.
        tag_value (float): The value of the tag to filter points.

    Returns:
        tuple: (min_y, max_y) for the points with the given tag.
    """
    with open(file_path, 'r') as file:
        lines = file.readlines()

    header = []
    data_start_index = 0
    for i, line in enumerate(lines):
        header.append(line.strip())
        if line.strip() == "DATA ascii":
            data_start_index = i + 1
            break

    # Extract fields and locate the tag column
    field_line = next((line for line in header if line.startswith("FIELDS")), None)
    if not field_line:
        raise ValueError("FIELDS line not found in PCD header.")
    fields = field_line.split()[1:]  # Ignore "FIELDS" keyword

    # Ensure 'y' and the tag field exist
    if "y" not in fields or tag_field not in fields:
        raise ValueError(f"Required fields ('y' or '{tag_field}') are missing in the PCD file.")

    y_index = fields.index("x")
    tag_index = fields.index(tag_field)

    # Read and filter data points
    min_y, max_y = float("inf"), float("-inf")
    for line in lines[data_start_index:]:
        values = re.split(r"\s+", line.strip())
        try:
            y = float(values[y_index])
            tag = float(values[tag_index])  # Adjust if tag is not numeric
            if tag == tag_value 
                min_y = min(min_y, y)
                max_y = max(max_y, y)
        except (ValueError, IndexError):
            continue  # Skip malformed lines

    if min_y == float("inf") or max_y == float("-inf"):
        raise ValueError(f"No points found with tag '{tag_value}' in field '{tag_field}'.")

    return min_y, max_y


if __name__ == "__main__":
    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="Filter PCD points by tag and find min/max Y values.")
    parser.add_argument("pcd_file", type=str, help="Path to the PCD file.")
    parser.add_argument("tag_field", type=str, help="Name of the tag field in the PCD file.")
    parser.add_argument("tag_value", type=float, help="Value of the tag to filter points.")

    args = parser.parse_args()

    # Process the PCD file
    try:
        min_y, max_y = read_ascii_pcd(args.pcd_file, args.tag_field, args.tag_value)
        print(f"Minimum Y value: {min_y}")
        print(f"Maximum Y value: {max_y}")
    except Exception as e:
        print(f"Error: {e}")
