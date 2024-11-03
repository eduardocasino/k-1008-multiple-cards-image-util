# 4-bit Grayscale palette for use with k-1008 grayscale images

## Prepare images with GIMP

1. Import palette files (`grays_4.gpl`, `grays_8.gpl`and `grays_16.gpl`):
    * From the Gimp menu, select `Windows->Dockable dialogs->Palettes`
    * Right-click on any palette and select `Import Palette...`
    * Click on `Palette file` and select the desired one and press `Import`
    * Repeat for the rest of the palettes

    Now you will see the imported palettes among the builtin ones:

2. Open or create a new image. Maximum image size is 320x200 pixels.

    Credit: Image by [Alexas_Fotos](https://pixabay.com/photos/lion-animal-mammal-predator-mane-3576045/)
3. Select `Image->Mode->Indexed...` from the menu
4. Select `Use custom palette` and choose one of the imported ones (for B&W just select `Use black and white (1-bit) palette`) Normally, you want to use dithering for better results.
5. Select `File->Export as..` and choose `C source code header`


## Import the palette to GIMP

Windows->Dockable dialogs->Palettes

Click on the palettes icon

Import palette file
(change name to whatever you want)

Scale image to 320x200

Image->mode->indexed

Select custom palette

Export file has C header


