General Purpose Input and Output

To make MINIX more usable on embedded hardware, we need some way to access the
GPIO features of the system on a chip. Generally System on Chips (SoC) designs
provide some way to configure pads to perform basic Input/Output operations on
selected ports. These ports are also usually grouped into banks. The end
result is that you have a functional general input output block where you need
to configure some the following functions.

Functional Requirements

We envision that the short term usage of the GPIO library will be both input
and output handling. Input handling as we want to be able to listen to button
presses and generate key events, and output handling because we want to be able
to control LEDs.

GPIO required functionality
-Configure pins as input or output.
-Configure the impedance of the pins.
-Get or set the values of the pins (possibly in a single call).
-Configure interrupt levels for input pins.
-Configure debouncing of pins.

Additional kernel requirements:
-Manage the GPIO resources (who may access what)
-Access the GPIO pins from within driver (for the keyboard)
-Access the GPIO pins from within userland (for toggling LEDs)


Usage: 
You have to manually mount the gpio fs using the following command

# mkdir -p /gpio
# mount -t gpio none /gpio

