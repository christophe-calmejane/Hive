### Example of how to generate the icon files
## From macOS with ImageMagick
- Install imagemagick (brew install imagemagick)
- Convert the PNG file to ICO format using the command:
```bash
magick <source_png> -resize 256x256 -background none -gravity center -extent 256x256 -define icon:auto-resize=256,128,64,48,32,24,16 <output_ico>
```

## From Windows
- Use Windows store [App Icon Generator](https://apps.microsoft.com/store/detail/app-icon-generator-icon-maker-studio/9NG751Z4T1ZG)

## From Web
- Create the ico file using [Online Icon Editor](https://redketchup.io/icon-editor)
