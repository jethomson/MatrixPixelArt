### Introduction

This is not a fork of WLED. This is a standalone project.

The WLED Pixel Art Converter ([1](https://kno.wled.ge/features/pixel-art-converter/), [2](https://github.com/Aircoookie/WLED/tree/main/wled00/data/pixart)) is a neat tool that will convert images to pixel art for display on an LED matrix. However I found how the pixel art fit into WLED as a whole to be limited.

This project adds support for images with transparency and adds a proxy color feature which can be replaced with other colors on the fly (see bottle_magic).

Supporting transparent images opens up the possibility of layering images and other effects to make cool composites.

Simple animated images can also be made by combining references to pixel art images, assigning them a duration, and ordering them in a sequence.

Playlists of images, composites, and animated images may also be created.

<br>

![screenshot showing the various features available in the web UI](https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/web_ui_front.png "Screenshot of web UI front page")


---
<br>

**Partial transparency and proxy color substitution composite example:**

![composite example showing partial transparency and proxy color changing](https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/partial_transparency_magic_ghost.gif "Example of a composite of a background effect and an image with partial transparency and a proxy color that is automatically replaced with shifting colors")


---
<br>

**Full transparency composite example:**

![composite of moving mushroom with full transparency](https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/mushroom.gif "Example of a composite of a mushroom with a transparent hole showing the background underneath")

---
<br>

**Animation example:**

![animated pixel art of Link's death](https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/link_death_medium.gif "Animation of Link's death made from several still images")

---
<br>

**Glow In The Dark:**<br>
The X-ray effects will reveal the layer beneath it pixel by pixel. When this is paired with a diffuser printed with glow in the dark filament the hidden image will be revealed when the effect stops.
![ghost glowing in the dark from xray effect](https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/ghost_glow.gif "Example of an X-ray effect that reveals hidden image pixel by pixel with a glow in the dark diffuser")


---
### Programming ESP32
There are two different ways you can load the code and example files onto the ESP32.

**Method 1**
<br>
[Use a browser that supports Web Serial and program the ESP32 board with this page.](
https://jethomson.github.io/MatrixPixelArt/webflash/flash.html)


**Method 2**
<br>
Download this repository and open it in PlatformIO.
Compile and upload the code.
Click the ant icon on the left hand side, under Platform, click Build Filesystem Image, then click Upload Filesystem Image.

### Hardware
[16x16 WS2812B Addressable RGB LED Matrix](https://www.amazon.com/dp/B097MBPZJW)
<br>
[ESP32 D1 Mini - USB C (purchased)](https://www.aliexpress.us/item/3256805791099168.html)
<br>
[ESP32 D1 Mini - USB C (not purchased, but look identical)](https://www.amazon.com/AITRIP-ESP-WROOM-32-Bluetooth-Internet-Development/dp/B0CYC227YG)
<br>
[5V, 5.5 mm x 2.5 mm Barrel Plug Power Supply](https://www.amazon.com/dp/B078RT3ZPS)
<br>
[5.5 mm x 2.5 mm Barrel Jack](https://www.amazon.com/dp/B07C4F7BP5)
<br>
<br>
The screw terminal connector that comes with the power supply or similar should be avoided. It makes a poor connection that will lead to the ESP32 resetting from lost power and the barrel connector getting hot.
<br>
Hot glue does not stick well to the flexible PCB, so I put double sided tape on the PCB and applied hot glue on top of that to provide strain relief and keep things stable.
<br>
The capacitor is not strictly necessary. I added it to troubleshoot the resetting issue which turned out to be caused by the poor screw terminal connector that came with the power supply so the capacitor was not the ultimate fix.
<br>
My ESP32 board requests a max current of 136 mA from my computer, so I am able to safely program and test with the LEDs drawing current. However your board and USB power supply may not set safe current limits. Ideally you would program the ESP32 before attaching the LED matrix, and only power the LEDs with the 5V power supply wires which do not pass current through the ESP32's thinner PCB traces.
<br>

ESP32 to LED matrix wiring
<br>
IO16 -- Green -- DIN 
<br>
 GND -- White -- GND
<br>
 VCC --  Red  --  5V 
<br>
<br>
<img src="https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/wiring.jpg" width="60%">

<br>

<img src="https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/live_mini_kit_esp32.png" width="60%">

<br>
Here is a 3D rendering of all parts of the enclosure.
The diffuser shown in green and the grid shown in orange are one part.
You should edit the model to add a color change where the grid starts.
The diffuser should be printed in white and the grid should be printed in black.
A diffuser printed from white filament looks better than one printed from clear filament.
The grid should be black to prevent light bleed and give the images displayed a distinctive pixel art look.
<br>
If you print the diffuser with two layers of glow in the dark filament and one layer of white you can use X-ray effects to reveal glow in the dark images!
<br>

<img src="https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/enclosure.png" width="60%">

<br>


### Initial Setup
The device can create its own WiFi network or it can connect to your established WiFi network.
You can control the device by connecting directly to its WiFi network, but you will be able to access the device more easily and be able to use the time and date features if you connect it to your WiFi network.

Find the WiFi network named PixelArt and connect to it.
The Configuration page will pop up automatically*.
Enter your WiFi network information (SSID and password).
The rest of the fields may be ignored for now.
Click Save.
The device should restart and connect to your WiFi network.
The PixeArt WiFi network should no longer be visible in your list of available networks.
It will appear again if the device no longer has access to your WiFi network (e.g. password changed).

Now any computer or phone on your WiFi network will be able to control the device be visiting pixelart.local.

*If the Configuration page does not pop up automatically after connecting to PixelArt try the following.
You may get a dialog message like: "The network has no internet access. Stay connected?"
If so, answer Yes. I do not recommend marking the box "[ ] Don't ask again for this network"
In a web browser, open the site pixelart.local or 192.168.4.1.
Open the Configuration page.

**Setting the timezone**

If you want to use UTC time then nothing further is required.

If you would like to use your local time:
<br>
Go to pixelart.local again and open the Configuration page.
<br>
Delete the information in the IANA Timezone field, and click outside of that field.
With luck your IANA Timezone will be filled in automatically based on the location of your IP address, then a few moments later the POSIX Timezone (has Daylight Saving Time info) will also automatically be filled in. If this information is not correct you can manually set the IANA Timezone, then the POSIX Timezone will automatically update.
Click Save and wait for the device to restart.
Now you should be able to add the time and date to your composites.

### Creating Art

**Creating Sprites - Magic Ghost**

Sprites can be extracted easily by using a 16x16 selection box.
<br>
<img alt="extracting ghost sprite in GIMP" src="https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/extract_ghost.png" description="Example of how to extract a sprite in GIMP using a fixed size 16x16 selection box" width="60%">

<br>
<br>
Manually entering a value of 3200% for the zoom value will make tiny images much easier to work with.
<br>
Use color picker to find the color you want to swap out using the Combine Effects (compositor) page.

<br>
<br>
Both complete and partial transparency works in Matrix Pixel Art. Complete transparency is good for removing backgrounds so other layers can be seen underneath. Partial transparency is good for making objects look like they are see-through.
<br>

<img alt="changing the transparency and capturing the body color of the ghost GIMP" src="https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/change_transparency.png" description="Example of changing the transparency and capturing the body color of the ghost for use as the proxy color" width="60%">

<br>
<br>
Here are the GIMP settings I used when saving a PNG that will be processed by the converter. 8bpc RGBA is the most important setting.
<br>
<img alt="GIMP settings used to save png" src="https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/gimp_settings.png" description="Settings I use for saving png that will be processed by the converter. 8bpc RGBA is the most important setting.">

<br>
<br>
If you wish for Matrix Pixel Art to replace one color in your image with other colors put the color you wish to be replaced in the Proxy Color field. You can use your image editor to find this color, but it isn't too hard to find the hex color value by looking at the text area that shows the image converted to JSON. Notice the output text: ffb7ae80. ffb7ae is the color of the ghost's body, and hex 80 (dec 128, 50% of 255) is the transparency amount.
<br>
<img alt="using image converter to save ghost and set proxy color" src="https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/converter_proxy_color.png" description="Example of converting ghost and setting the proxy color to the value we found in the previous step" width="60%">

<br>
<br>
Here's what it looks like when you save an image with the Proxy Color set. The proxy color will be replaced by a loop of rainbow colors. The compositor will let you replace the proxy color with any color. So instead of saving separate red, pink, cyan, and orange ghosts you can just use the converter to save one ghost with the proxy color set and use the compositor to create four different colored ghosts from one image.

![peach ghost with proxy color set shows rainbow colors on save](https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/magic_ghost_proxy_color_save.gif "Example of how setting the proxy color allows the image to change color automatically")

**Preparing Images For Conversion**

Not everything you display on Matrix Pixel Art needs to already be a sprite or tiny pixel art.
Some images are still surprisingly recognizable when scaled down.
The image converter will automatically scale down images to fit on the display, but I prefer to scale the images down manually in an image editor and adjust the levels before uploading them using the converter.
I get good results when using either the Cubic or Linear interpolation methods when scaling down.
I also like to adjust the levels by pulling in the the black and white sliders (leftmost and rightmost triangles) and move the midtone slider to the right to darken the image. I find that the display makes the images too bright when this step is skipped.
<br>
<img alt="an image in GIMP being scaled and modified in preparation for conversion" src="https://raw.githubusercontent.com/jethomson/jethomson.github.io/refs/heads/main/MatrixPixelArt_images/image_prep.png" description="Example of scaling other images and adjusting levels in preparation for conversion" width="60%">
