import cv2
import sys

def show_pixel_value(event, x, y, flags, param):
    """
    Callback function to print pixel values when the left mouse button is clicked.
    """
    if event == cv2.EVENT_LBUTTONDOWN:
        # Get the pixel value
        pixel_value = param[y, x]
        print(f"Pixel at ({x}, {y}): {pixel_value}")

def main(image_path):
    """
    Main function to load the image and set up the OpenCV GUI for pixel value visualization.
    """
    # Load the image in grayscale
    image = cv2.imread(image_path, cv2.IMREAD_UNCHANGED)
    if image is None:
        print(f"Error: Could not load image from path '{image_path}'")
        sys.exit(1)

    # Check the shape of the image
    if image is not None:
        print(f"Image shape: {image.shape}")  # (Height, Width, Channels)
        if image.shape[2] == 4:
            print("Image is loaded in BGRA format.")
        else:
            print("Image does not have an alpha channel.")
    else:
        print("Failed to load the image.")

    # Display the image in a window
    cv2.imshow('Image', image)

    # Set up the mouse callback function
    cv2.setMouseCallback('Image', show_pixel_value, image)

    # Wait until the user presses a key
    print("Click on the image to get pixel values. Press any key to exit.")
    cv2.waitKey(0)
    cv2.destroyAllWindows()

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script_name.py <image_path>")
        sys.exit(1)

    # Get the image path from the command-line argument
    image_path = sys.argv[1]

    # Call the main function
    main(image_path)
