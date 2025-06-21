### Introduction

This is not a fork of WLED. This is a standalone project.

The WLED Pixel Art Converter ([1](https://kno.wled.ge/features/pixel-art-converter/), [2](https://github.com/Aircoookie/WLED/tree/main/wled00/data/pixart)) is a neat tool that will convert images to pixel art for display on an LED matrix. However I found how the pixel art fit into WLED as a whole to be limited.

This project adds support for images with transparency and adds a proxy color feature which can be replaced with other colors on the fly (see bottle_magic).

Supporting transparent images opens up the possibility of layering images and other effects to make cool composites.

Simple animated images can also be made by combining references to pixel art images, assigning them a duration, and ordering them in a sequence.

Playlists of images, composites, and animated images may also be created.


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

### Initial Setup
The device can create its own WiFi network or it can connect to your established WiFi network.
You can control the device by connecting directly to its WiFi network, but you will be able to access the device more easily and be able to use the time and date features if you connect it to your WiFi network.

Find the WiFi network named PixelArt and connect to it.
You may get a dialog message like: "The network has no internet access. Stay connected?"
If so, answer Yes. I do not recommend marking the box "[ ] Don't ask again for this network"
In a web browser, open the site pixelart.local or 192.168.4.1.
Open the Configuration page.
Enter your WiFi network information (SSID and password).
The rest of the fields may be ignored for now.
Click Save.
The device should restart and connect to your WiFi network.
The PixeArt WiFi network should no longer be visible in your list of available networks.
It will appear again if the device no longer has access to your WiFi network (e.g. password changed).

Now any computer or phone on your WiFi network will be able to control the device be visiting pixelart.local.

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
