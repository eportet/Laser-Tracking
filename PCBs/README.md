# PCBs for laser tracking
We've got two PCBs to make the sensor for the drone.
The **sensor** board holds 24 photodiodes and sticks the info on a 26-pin header. The outputs require pullup resistors.
The **mux** board attaches to the sensor board, and is also designed to hold a particle photon and a RF communication module.

The kicad files, digikey bill of materials, and gerbers are in each of the directories. BOM for 3 boards + a spare, which is the minumum oshpark order.
We printed both boards with oshpark and it worked out well enough. "Sensor" is on 2oz copper, "mux" is standard.
