# File structure

This folder consists of various python files and two folders, holding a PlatformIO project and Vivado project.

## Misc files

- bitmaskConversion.py takes screenshots of lots of small pieces of the pattern we want to make from klayout and puts it in a folder with names that tell us where every image goes (the name isn't the most accurate one for it but whatever)
- cleanImages.py removes the black squares on the siemens star in the middle because no matter what I did on bitmaskConversion it would still stay somehow
- commandToTeensy.py for sending i2c commands to the DLPC
- computerSelectors.py a collection of helper functions that lets the user choose the COM port, a specific image file, or a folder of images generated from bitmaskConversion
- imageStitcher.py is a purely standalone file that takes the folder of images bitmaskConversion generates and pieces them together to see the full image
- LithoTestMask.gds a klayout test pattern that consists of a crosshair for (0,0), corner markers to show repeatability of patterns, many characterization patterns to see our lithographer's resolution for diagonals and horizontal/vertical lines, a siemens star, and dot arrays of various sizes.
- miscFuncs.py stores the crc functions from TI's specifications

main.py takes commands from the input() function and either sends an image, folder of images, i2c command to DLPC, or switches to a new port.

## TeensyStreaming

PlatformIO project for the teensy to perform two primary operations: 

- Take the processed image from a computer via serial and send it to the FPGA via SPI
- Communicate with the DLPC to change modes and perform other operations such as intializing it

It has two implementation files with helper functions to help with these operations:

- TeensyToDLPC
- TeensyToFPGA

The names are self-explanatory

main.cpp takes commands via serial from the computer and performs one of the two operations based off that.

## Everything

The FPGA modules that we are using, it consists of the following modules:

- SPIslave for getting image data from the teensy
- i2cslave for getting commands from the DLPC chip
- FIFO for storing image data in a fifo buffer
- Pvideo for converting the image data to the accepted form for the DLPC
- 

# Specifics

## bitmaskConversion.py

Makes a folder called klayout_tiles on the user's desktop

The default image proportions are 1280 x 720 because that's the size of the DMD we're using to make the patterns on the silicon wafer

There's also a margin variable that lets the user include a micrometer length margin outside of where there's a pattern just for visibility. However, with the corner markers it's not necessary

Then, it zooms in to a level where features of every pattern size is visible (which we found to be 9)

After that, it splits the grid into smaller sections, forces the patterns to be solid fill, and saves an image.

## cleanImages.py

This is solely because I couldn't get rid of the black squares in the LithoTestMask.gds file for the siemen star pattern, all it does it get rid of black pixels on images. The function takes an input and output path as arguments, and also an optional argument for how "black" (close to 0) the RGB value should be. Defaults to 20 out of 255.

## commandToTeensy.py

This file has a function that will send hex bytes and prompt the teensy to send those to the DLPC, which will allow us to perform functions that aren't hardcoded (like more than just streaming video)

All it does is read from input and converts it into a format that the teensy accepts

## computerSelectors.py

This file has a function that uses the tkinter package to select a COM port that the teensy is selected to. It also has a function that lets the user pick an image path, or a folder path (usually the folder that bitmaskConversion.py creates)

## imageStitcher.py

This file takes all the images with labeled coordinates in the folder that bitmaskConversion.py creates and stitches them together for the user to see if the pattern is correctly cleaned and sliced.

It uses the select_folder_ui() function that computerSelectors creates, and then uses the image coordinates (labeled by bitmaskConversion.py) to stitch them to a big image and save it in the same folder.

## imageToTeensy.py

### sendImage()

This function takes the path of the image the user wants to send, which com port to send it from, the baud, timeout, and how much information to send in one transmission as the arguments.

First, it cleans the image, converts it to grayscale, converts the image data to an numpy array, then checks if the size is 1280 x 720.

Then it flattens the array into a 1D byte stream.

After opening the serial port with the arguments, it waits a bit for the teensy to reset, and tells the teensy that it's sending an image and waits for an ACK. If that fails, it just returns -1 as a fail.

If it gets the ACK successfully, it starts the transmission.

While the amount of sent bytes is less than the total amount, we take a chunk of 512 bytes to prepare the send. Right now it's 512 because that's the max size packet that USB hardware can send.

For every chunk, we write it through serial and then check for an ACK, failing if the ACK is wrong. It sends a progress update of how many bytes have been sent every 20 chunks.

After the image is done sending, it calculates a crc from the whole image data and sends it to the teensy to check with the teensy and FPGA, and prints out which checks were successful.

### sendFolderOfImages()

This function takes the same arguments of sendImage() but instead of an image path it's a folder path.

For every file in the folder, it searches the name of the image for the coordinates, and adds that to an array. Then, it sorts the coordinates so that the images go from left to right and then down to up, and sends images in that order.

## miscFuncs.py

This stores the CRC calculation function as per the TI specs.

## main.py

It first initializes the port we're using to communciate with the teensy, and then waits for user input.

Currently, it can perform these operations:

Send (I)mage, (F)older, (C)ommand. OR (Q)uit or use new (P)ort: 

All of these commands should be self explainatory, and they use all the methods previously explained.