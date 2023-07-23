# N64 USB Mouse

A simple prototype for connecting a usb mouse to the N64

I used an Arudino Nano for the microcontroller

I used the CH375B chip to handle the USB host [Amazon Link](https://www.amazon.com/gp/product/B07CKVDW1W/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1)

## Pin Connections

The following arduino pins are connected to

* 0
* 1
* 2 - N64 Controller Signal Wire
* 3 - CH375B INT
* 4 - CH375B RD
* 5 - CH375B WR
* 6 - CH375B A0
* 8 - CH375B D0 
* 9 - CH375B D1
* 10 - CH375B D2 
* 11 - CH375B D3
* 12 - CH375B D4 
* A0 - CH375B D5
* A1 - CH375B D6
* A2 - CH375B D7

Becuase of limitations of the Ardunio, I had to split the data bus between two ports.

You should also wire the CH375B CS pin to ground