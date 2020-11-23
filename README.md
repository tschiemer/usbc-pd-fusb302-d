# usbc-pd-fusb302-d
Module for ONSEMI FUSB302-D (USB-C Controller) for PD negotiation with STM32 HAL-based driver.

https://github.com/tschiemer/usbc-pd-fusb302-d

The functional core is given, but might yet need some adaption, notable points:

- Does not use fusb302 interrupt but relies on register readout instead, which is really suboptimal and should be implemented in an optimal world.
- I2C abstraction and fusb302 driver was designed for the STM32 HAL.
- The magic happens in file `CCHandshake.c`, in particular functions `pd_onSourceCapabilities` and `pd_createRequest` which you might want to adapt to your desired power configuration - otherwise **you might damage connected components** (as it is set to get 60W or so). You might have to understand the possibilities PD has (see )
- You might have to implement some platform functions like `DelayMs(..)` or `DBG(..)` and set some defines , I'm sure you'll figure it out.
- This module could use a bit of love.

## Using

if everything is configured, you'll just have to add something like this:

```c

// general system init
HW_I2C_Init();
CCHandshake_init();

// core loop
while(1){

  // ...

  // if (fusb302 IRQ)
  // check if there is something to be done and handle events accordingly
  CCHandshake_core();

  // ...

}

```

`CCHandshake_core()` is roughly doing the following:
1. detect insertion/removal and orientation of USB-C
2. on detection: request source capabilities and negotiate for desired capability.

## Resources

- https://www.onsemi.com/products/interfaces/usb-type-c/fusb302
- https://www.usb.org/document-library/usb-power-delivery
- https://www.embedded.com/design/power-optimization/4458400/USB-Type-C-and-power-delivery-101-----Power-delivery-protocol
- http://blog.teledynelecroy.com/2016/05/usb-type-c-and-power-delivery-messaging.html


## License

Copyright (c) 2020 Philip Tschiemer, [filou.se](https://filou.se)

GNU Lesser General Public License v3.0

Also see `LICENSE.txt`
