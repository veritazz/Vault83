# NameToBeFound

ArduboyFX game at an early stage.

# Installation

Install platformio as described here

http://docs.platformio.org/en/latest/installation.html#installation-methods

Clone the following two repositories into the *lib* directory:

```
https://github.com/veritazz/Arduboy2.git (branch fps_development)
https://github.com/veritazz/ATMlib2.git (branch veritazz-wip)
```

After that the datafile for the flash memory should be created. The script that creates it will
also create .c and .h files that are required for compilation. As long as assets like graphics,
sound and maps do not change the below step needs to be done only once:

```
./create_data.sh
```

To build the program file run

```
platformio run
```

Now *.pio/build/leonardo/firmware.hex*  and the data file *data-output/Vault83.bin* can be
added to a flashcart collection
