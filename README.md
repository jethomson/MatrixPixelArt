This is not a fork of WLED. This is a standalone project.

The WLED Pixel Art Converter ([1](https://kno.wled.ge/features/pixel-art-converter/) [2](https://github.com/Aircoookie/WLED/tree/main/wled00/data/pixart)) is a neat tool that will convert images to pixel art for display on an LED matrix. However I found how the pixel art fit into WLED as a whole to be limited. 

This project adds support for images with transparency and adds a proxy color feature which can be replaced with other colors on the fly (see bottle_magic).

Supporting transparent images opens up the possibility of layering images and other effects to make cool composites.

Simple animated images can also be made by combining references to pixel art images and assigning them a duration.

Playlists of images, composites, and animated images may also be created.


Partial transparency and proxy color substitution composite example

![composite example showing partial transparency and proxy color changing](https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt/partial_transparency_magic_ghost.gif "Example of a composite of a background effect and an image with partial transparency and a proxy color that is automatically replaced with shifting colors")


Full transparency composite example

![composite of moving mushroom with full transparency](https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt/mushroom.gif "Example of a composite of a mushroom with a transparent hole showing the background underneath")


Animation example

![animated pixel art of Link's death](https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt/link_death_medium.gif "Animation of Link's death made from several still images")
