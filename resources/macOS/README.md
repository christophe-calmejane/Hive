### Example of how to generate the icon files
## macOS
For each size, run the following commands in the terminal:

```bash
SRC="icon.png" # Path to your PNG file
DEST="iconset" # Path to the folder where you want to save the icon files
size_px=16 # Size in pixels (16, 32, 128, 256, 512, 1024)
filename="icon_${size_px}x${size_px}.png" # Name of the output file
sips -Z $size_px "$SRC" --out "/tmp/$filename" >/dev/null
sips -p $size_px $size_px "/tmp/$filename" --out "$DEST/$filename" >/dev/null
```

## Other
- Get the PNG file of your icon (256 or 512)
- Create the icns file using [image2icon by Shiny Frog](https://apps.apple.com/fr/app/image2icon/id992115977) or any other software
