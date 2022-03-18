This is a sample Intel USB camera minidriver.  A USB camera minidriver is a kernel
mode driver that serves to abstract the rest of the WDM video capture
subsystem from the specifics of an individual USB camera hardware.
This sample driver works in conjunction with USBCAMD (source included in DDK).
It was written according to WDM video capture spec as well as USBCAMD sepc.

This minidriver supports both of Intels's Create & Share USB cameras (Models YC72, YC76).

The code is provided mainly as a reference for people writing
minidrivers to support other usb cameras .  

The INF file is provided mainly to illustrate the registry
modifications required to make the minidriver visible to other
WDM/KS components.

This code has been updated to use the new interfaces in USBCAMD version 2. The define USBCAMD2 flag in 
the source files reflects that. This new version of the sample driver exposes an additional still pin. Hence, if 
you are using Intel camera model YC76, you will be able to take a snapshot eveytime you press the snapshot on the camera.
To do that, insert the Intel camera filter in graphedt. You will see two pins: capture, still.
Right click on the mouse on each pin and render, then press the play button. Once the video starts streaming on the
video renderer, press the snapshot button, and you will see a still coming up on the video renderer connected to the still
button.




