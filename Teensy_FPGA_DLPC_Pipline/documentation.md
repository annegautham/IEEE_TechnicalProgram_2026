# File structure

This folder consists of various python files and two folders, holding a PlatformIO project and Vivado project.

## Misc files

- bitmaskConversion.py takes screenshots of lots of small pieces of the pattern we want to make from klayout and puts it in a folder with names that tell us where every image goes (the name isn't the most accurate one for it but whatever)
- cleanImages.py removes the black squares on the siemens star in the middle because no matter what I did on bitmaskConversion it would still stay somehow
- commandToTeensy.py for sending i2c commands to the DLPC
- computerSelectors.py a collection of helper functions that lets the user choose the COM port, a specific image file, or a folder of images generated from bitmaskConversion
- imageStitcher.py is a purely standalone file that takes the folder of images bitmaskConversion generates and pieces them together to see the full image
- LithoTestMask.gds a klayout test pattern that consists of a crosshair for (0,0), corner markers to show repeatability of patterns, many characterization patterns to see our lithographer's resolution for diagonals and horizontal/vertical lines, a siemens star, and dot arrays of various sizes.
- miscFuncs.py stores the CRC16 functions from TI's specifications

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

## Misc files

### bitmaskConversion.py

Makes a folder called klayout_tiles on the user's desktop

The default image proportions are 1280 x 720 because that's the size of the DMD we're using to make the patterns on the silicon wafer

There's also a margin variable that lets the user include a micrometer length margin outside of where there's a pattern just for visibility. However, with the corner markers it's not necessary

Then, it zooms in to a level where features of every pattern size is visible (which we found to be 9)

After that, it splits the grid into smaller sections, forces the patterns to be solid fill, and saves an image.

### cleanImages.py

This is solely because I couldn't get rid of the black squares in the LithoTestMask.gds file for the siemen star pattern, all it does it get rid of black pixels on images. The function takes an input and output path as arguments, and also an optional argument for how "black" (close to 0) the RGB value should be. Defaults to 20 out of 255.

### commandToTeensy.py

This file has a function that will send hex bytes and prompt the teensy to send those to the DLPC, which will allow us to perform functions that aren't hardcoded (like more than just streaming video)

This first checks if there are parameters. Then, it writes 'W' to the teensy to make it receive i2c commands, and checks the ACK to make sure it's correct.

After that, it sends the first section which is the opcode and number of parameters.

If there are parameters, it writes those as well and waits a short time for the teensy to print back debug outputs.

Finally, it reads the ACK to make sure everything went smoothly.

### computerSelectors.py

This file has a function that uses the tkinter package to select a COM port that the teensy is selected to. It also has a function that lets the user pick an image path, or a folder path (usually the folder that bitmaskConversion.py creates)

### imageStitcher.py

This file takes all the images with labeled coordinates in the folder that bitmaskConversion.py creates and stitches them together for the user to see if the pattern is correctly cleaned and sliced.

It uses the select_folder_ui() function that computerSelectors creates, and then uses the image coordinates (labeled by bitmaskConversion.py) to stitch them to a big image and save it in the same folder.

### imageToTeensy.py

#### sendImage(IMAGE_PATH, ser, CHUNK)

This function takes the path of the image the user wants to send, the serial object, and how much information to send in one transmission as the arguments.

First, it cleans the image, converts it to grayscale, converts the image data to an numpy array, then checks if the size is 1280 x 720.

Then it flattens the array into a 1D byte stream.

After opening the serial port with the arguments, it waits a bit for the teensy to reset, and tells the teensy that it's sending an image and waits for an ACK. If that fails, it just returns -1 as a fail.

If it gets the ACK successfully, it starts the transmission.

While the amount of sent bytes is less than the total amount, we take a chunk of 512 bytes to prepare the send. Right now it's 512 because that's the max size packet that USB hardware can send.

For every chunk, we write it through serial and then check for an ACK, failing if the ACK is wrong. It sends a progress update of how many bytes have been sent every 20 chunks.

After the image is done sending, it calculates a CRC16 from the whole image data and sends it to the teensy to check with the teensy and FPGA, and prints out which checks were successful.

#### sendFolderOfImages(selected_folder, ser, CHUNK)

This function takes the same arguments of sendImage() but instead of an image path it's a folder path.

For every file in the folder, it searches the name of the image for the coordinates, and adds that to an array. Then, it sorts the coordinates so that the images go from left to right and then down to up, and sends images in that order.

### miscFuncs.py

This stores the CRC16 calculation function as per the TI specs.

### main.py

It first initializes the port we're using to communciate with the teensy, and then waits for user input.

Currently, it can perform these operations:

Send (I)mage, (F)older, (C)ommand. OR (Q)uit or use new (P)ort: 

All of these commands should be self explainatory (using tkinter to select and then passing arguments to the functions), and they use all the methods previously explained.

Sending i2c is a bit different, as it still uses input(). The format of commands should be hex bytes separated by a space. After receiving everything, it splits the string into each separate byte and converts to integers, and passes those arguments into the function. 

## TeensyStreaming

### TeensyToDLPC.cpp

This contains helper functions for communicating with the DLPC chip through i2c protocol.

#### writeDLPC(uint8_t opCode, uint8_t* params, uint8_t len)

This function takes in arguments opCode, the hex value you are writing to (register address, basically), the parameter(s) (arguments given to that register), and the length of the parameter(s) you are sending in bytes.

It first sends the DLPC's address given by TI, specifies the register, and then sends the parameter and returns if it's a success or not. I'm using a [Teensy 4.x i2c library](https://github.com/Richard-Gemmell/teensy4_i2c) that uses wire and has more readable API, and simpler usage.

#### setOperatingMode(DLPC_Mode mode)

This function is just a wrapper of writeDLPC() because it is used a lot, it sets the DLPC to either test pattern generation, splash pattern, external parallel video mode, or standby mode.

#### setStandby()

This is a wrapper of the above function to set the DLPC to standby mode, which is used the most in sending images to the DLPC

#### initDLPC(int irqPin)

This function sets up i2c and the DLPC for operation according to TI's programmer guide. It takes in the pin that provides the completion status of the DLPC system initialization as an argument.

It first waits for the DLPC to pull the irq pin low which signals that it's done initializing. Then, it initializes i2c communication using the library previously mentioned at 100kHz clock speed from TI.

Then, it follows the section 3.3.2 3D print procedure with FPGA front-end:

Configuring FPGA first: error injection, enable CRC16, reset FPGA, allow reset

Setting the active buffer to 0 (the way these buffers work is you first load one completely, and then switch to the other buffer. Then, the DLPC takes the inactive buffer and sends it to be printed)

Then, configure external print to have linear degamma and use LED 1, which we are using.

Now the DLPC is ready to have images sent to it.

### TeensyToFPGA.cpp

#### send_header(bool &isFirst)

This function is to satisfy section 4.2 in the programmers guide, where at the start of every image transmission you have to send the length of the entire data being sent for the image over all transmissions. We need this because we are dividing SPI data because the FPGA's buffer we're using isn't large enough, and also the teensy's buffer isn't large enough to hold the whole image all at once.

Each SPI transmission must have the opCode (0x04), the row and column index, a dummy byte of all 0s, and the length (if it's the first SPI transmission)

#### crc16_update(uint16_t crc, uint8_t *data, uint32_t len)

Because the teensy buffer isn't large enough to hold the entire image, we have to calculate the checksum progressively across every chunk. This function does that using the parameters that TI uses.

### main.cpp

Currently, main.cpp has a lot of stuff and I plan on moving some to functions and cleaning it up more.

At the moment, it first initializes variables such as the parameters for the CRC16 function, total length of transmission, pins, buffer, and things for i2c transmission.

It first initializes UART with the computer at a baud rate of 115200, initializes the DLPC, and writes the chip select pin for the FPGA high. Then, it intializes SPI at 20 MHz (slightly slower for testing and we don't have to do anything with the FPGA to make it the clock go faster than 100 MHz).

The main loop uses a state machine to choose what operation it performs, as it's faster than using if statements by a lot.

The states it can switch between are:

- Waiting for a command ((I)mage or (W)rite to DLPC)
- Receiving an image
- Writing to a register
- Writing the parameters of a register

#### Waiting for command

This state waits for serial to be available, and reads either and I or W, and switches the mode to either receiving image or writing i2c respectively. It also sends a unique ACK to make sure it got the correct mode. If it's I, it resets all the variables used for the first SPI transmission.

#### Receiving image

This state first reads either a chunk of bytes, or however much is remaining (whichever one is less).

Then, it updates the CRC16 using crc16_update().

After sending the header, the teensy transfers the chunk of the image via SPI to the FPGA, updates the amount sent, and sends an ACK back to the computer to let it know it can send more.

Once we have sent the last chunk, we check the two CRC16 bytes sent from the computer with what we have calculated. If this is correct, we then transfer both bytes of the CRC16 via SPI to the FPGA, and see the response the FPGA sends, and send it back to the computer. 

If the teensy fails the CRC16, we don't check the FPGA response because it the calculation should be the same, and if the teensy failed then it wouldn't be sending useful data to the FPGA.

After everything it turns back into the waiting for command state

We need a way to cancel image transmission if it goes wrong and tell the FPGA to not use the bad data.

#### Writing register

This state waits for two bytes from the computer via serial. One marks the current opCode we are sending, and the other is how many bytes the parameters we're sending are.

This is because we don't know when the parameters are full, and going through every command and mapping that to a parameter length isn't robust enough. Instead of writing an end byte or some pattern to signify that (which might repeat with a parameter somehow), we instead ask for the length and then move to our next state of writing parameters

#### Writing parameters

This waits for any serial, adds it into a buffer defined before for parameters one byte at a time, and once that buffer length is greater than or equal to the length defined above, we send it and send an ACK.

There's also optional debug to see what we are sending to the DLPC.

After everything it turns back into the waiting for command state.