# rf-ctrl - A command-line tool to control 433MHz OOK based devices


## Synopsis

rf-ctrl is a command line tool allowing to control wireless plugs and chimes that use a 433MHz OOK modulation.
The OOK modulation (ON/OFF Keying) allows to transmit binary data on a particular frequency by enabling or disabling the transmitter oscillator. I will call these states High and Low.

A big part of the wireless plugs that can be found on the market uses this modulation with a carrier frequency of 433.92MHz, and almost all of them follow the same frame pattern regarding the transmitted data:

- a starting marker : High to Low transition with particular High and Low durations
- the actual binary data : 0 and 1 bits are coded using High to Low transitions, with different High and Low timings
- an ending marker : High to Low transition with particular High and Low durations

This frame is then repeated a specific number of times. Some protocols are based on High to Low transitions, while some others use Low to High transitions.

rf-ctrl uses a frontend/backend logic allowing to generate the proper frame for a particular protocol on one side, and to use a particular 433MHz enabled hardware to send it on the other side.
This means that adding a new transmitter driver will allow all the supported protocols to be used out of the box.


## Build instruction

As usual:
```
$ make
```

Take a look at the __Makefile__ to see the variables that can be used (for instance CROSS_COMPILE).


## OpenWrt support

In order to build rf-ctrl as an OpenWrt package and/or add it to an OpenWrt firmware build:
- checkout an OpenWrt build tree (only tested on Barrier Breaker 14.07 so far)
- create the __openwrt/package/utils/rf-ctrl__ sub-folder and place __OpenWrt/Makefile__ in it
- create the __openwrt/package/utils/rf-ctrl/src__ sub-folder and place all the files from the __rf-ctrl__ root folder in it

Now, if you run `$ make menuconfig` from the __openwrt__ root folder, you should see an __rf-ctrl__ entry in the __Utilities__ sub-menu !


## Currently supported protocols and transmitters

The supported transmitters are:
- HE853 : the ELRO/Home Easy USB dongle
- OOK-GPIO : any 1$ 433MHz transmitters connected to a GPIO ([ook-gpio](https://github.com/jcrona/ook-gpio) kernel driver required)

The supported protocols are:
- DI-O
- Home Easy (not tested because I don't have one)
- Idk (Chinese wireless chime that is probably sold under different brands)
- OTAX (found in a the French Conforama store)
- Auchan (found in the French Auchan store, and probably sold under different brands)
- Auchan2 (also found in the French Auchan store, and probably sold under different brands)
- Sumtech (not sure were it can be found, it is a pure chinese product like Idk)
- Blyss (found in french Castorama store)
- Somfy (RTS 433,42 MHz only)


## Usage

Turning off the DI-O device number 3 paired to the remote ID 424242:
```
$ sudo ./rf-ctrl -p dio -r 424242 -d 3 -c off
```

Starting a fast RF scan on every OTAX devices, sending the 'on' command:
```
$ sudo ./rf-ctrl -p otax -c on -s -n 1
```


## License

rf-ctrl is distributed under the GPLv2 license. See the LICENSE file for more information.


## Miscellaneous

Thanks to r10r and his [he853-remote](https://github.com/r10r/he853-remote) project. It saved me a lot of time when I implemented the HE853 USB dongle driver and the Home Easy protocol.
So far, rf-ctrl has been successfully tested on a regular Linux PC, a TP-Link TL-WR703N router running OpenWrt, and a Raspberry Pi.
Check out my [Home-RF](https://github.com/jcrona/home-rf) project for a simple Web-UI that uses rf-ctrl.

Fell free to visit my [blog](http://blog.rona.fr), and/or send me a mail !

__  
Copyright (C) 2016 Jean-Christophe Rona
