# K-1008 multiple cards image utility

## About

This utility converts GIMP images in C source code header format to a format suitable for display on a KIM-1 with one to four K-1008 cards as described in the ["Use of the K-1008 for grey scale display, app note #2"](http://retro.hansotten.nl/uploads/mtu/MTU%20App%20Note%20Gray%20Scale%20K-1008.pdf) document.

## Usage

Prepare the image to be converted as described in the last section of this document. Maximum file size is 320x200 pixels.

```
$ kimg -i <input_file> [ -o <output_file> ] [-p <palette_file>] [ -f <format> ] [ -a <hex_base_addr> ]

        Supported formats:

        pap     - MOS Papertape (default)
        ihex    - Intel HEX
        asm     - CA65 assembly code
```

* The only mandatory argument is the input file.
* If no output file name is provided, it will be composed with the base name of the input file, followed by the output format as its extension.
* The palette file must correspond to the one used to prepare the image. If no file is provided, 1-bit black & white is assumed.
* If no format is specified, PAP is assumed.
* Default base address is 2000. Minimum is 2000, maximum is A000.

## Compile

Only unix-like systems (including WSL) with the GNU C toolchain are supported.

To build, just type
```
$ make
```

## Prepare images with GIMP

1. Import palette files (`grays_4.gpl`, `grays_8.gpl`and `grays_16.gpl`):
    * From the Gimp menu, select `Windows->Dockable dialogs->Palettes`

        ![palette-dialog](https://github.com/eduardocasino/k-1008-multiple-cards-image-util/blob/main/images/palette-dialog.png?raw=true)
    * Right-click on any palette and select `Import Palette...`

        ![palette-import](https://github.com/eduardocasino/k-1008-multiple-cards-image-util/blob/main/images/palette-import.png?raw=yes)
    * Click on `Palette file` and select the desired one and press `Import`

        ![palette-import-2](https://github.com/eduardocasino/k-1008-multiple-cards-image-util/blob/main/images/palette-import-2.png?raw=yes)
    * Repeat for the rest of the palettes

        Now you will see the imported palettes among the builtin ones:

        ![palette-import-3](https://github.com/eduardocasino/k-1008-multiple-cards-image-util/blob/main/images/palette-import-3.png?raw=yes)

2. Open or create a new image. Maximum image size is 320x200 pixels.
3. Select `Image->Mode->Indexed...` from the menu

    ![palette-import-3](https://github.com/eduardocasino/k-1008-multiple-cards-image-util/blob/main/images/mode-dialog.png?raw=yes)
4. Select `Use custom palette` and choose one of the imported ones (for B&W just select `Use black and white (1-bit) palette`) Normally, you want to use dithering for better results.

    ![palette-import-3](https://github.com/eduardocasino/k-1008-multiple-cards-image-util/blob/main/images/mode-indexed.png?raw=yes)
5. Select `File->Export as..` and choose `C source code header`

    ![palette-import-3](https://github.com/eduardocasino/k-1008-multiple-cards-image-util/blob/main/images/export-dialog.png?raw=yes)
Lion image credit: [Alexas_Fotos](https://pixabay.com/photos/lion-animal-mammal-predator-mane-3576045/)
