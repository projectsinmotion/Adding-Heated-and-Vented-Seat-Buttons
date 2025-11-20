# Hardware

## Off the Shelf Hardware
- **Board:** [ESP32S3 CAN & LIN-Bus Board](https://www.skpang.co.uk/products/esp32s3-can-and-lin-bus-board)
    - **Vendor:** [Copperhill Technologies](https://copperhilltech.com/esp32s3-can-lin-bus-board/)
    - **LIN Transceiver:** [TJA1021T](https://www.nxp.com/part/TJA1021T)
    - **CAN Transceiver:** [SN65HVD230](https://www.ti.com/product/SN65HVD230)
    - **Bolts to Secure Board to Enclosure Body (4 per Project):** [Metric socket cap, Class 12.9 alloy steel black oxide finish, 2mm x 0.4mm x 5mm](https://boltdepot.com/Product-Details?product=13322)
- **Button Modules:** 
    - **Driver's Side Heated and Vented Seat Button Module:** [Mopar 6EM18TX7AB](https://store.mopar.com/oem-parts/mopar-heated-seat-switch-6em18tx7ab)
    - **Passenger's Side Heated and Vented Seat Button Module:** [Mopar 6EM19TX7AB](https://store.mopar.com/oem-parts/mopar-heated-seat-switch-6em19tx7ab)
    - **Bolts to Secure Mounting Flanges to Mounting Bases (4 per Project):** [Metric socket cap, Class 12.9 alloy steel black oxide finish, 3mm x 0.5mm x 10mm](https://boltdepot.com/Product-Details?product=13636)
    - **Adhesive for Bonding Mounting Flanges to Button Modules and Mounting Bases to Faceplate:** [JB Weld Plastic Bonder - Black](https://www.jbweld.com/product/j-b-plastic-bonder-syringe)

## Modifications to Off the Shelf Hardware
The terminal connectors included on the ESP32S3 CAN & LIN-Bus Board have a 3.5mm pitch, and there do not seem to be any matching plugs available that use crimp terminals. As this project is an automotive application and the board will be subject to the vibrations and thermal variations of an automotive interior, crimped terminals are preferred over screw terminals. What are available, however, are the same style terminal connectors except with a 3.81mm pitch and matching plugs that use crimp terminals.
- **4 Position 3.81mm Pitch Pluggable Terminal Block** [Phoenix Contact 1803293](https://www.mouser.com/ProductDetail/Phoenix-Contact/1803293?qs=8BCRtFWWXOTid0RCpvM7uQ%3D%3D)
- **4 Position 3.81mm Pitch Plug** [Phoenix Contact 1852192](https://www.mouser.com/ProductDetail/Phoenix-Contact/1803293?qs=8BCRtFWWXOTid0RCpvM7uQ%3D%3D)
- **Terminals** [Phoenix Contact 1859991](https://www.mouser.com/ProductDetail/Phoenix-Contact/1859991?qs=Xflg4jNzfa6JFQqkZRuZkg%3D%3D)

If you would like to replace the LIN bus connector not only with a 3.81mm pitch connector but one that only has 3 terminals, you'll need the following:
- **3 Position 3.81mm Pitch Pluggable Terminal Block** [Phoenix Contact 1803280](https://www.mouser.com/ProductDetail/Phoenix-Contact/1803280?qs=wd%252Bw3mUqFrn4%252Bd94GrInMA%3D%3D)
- **3 Position 3.81mm Pitch Plug** [Phoenix Contact 1852189](https://www.mouser.com/ProductDetail/Phoenix-Contact/1852189?qs=EgthMTITkLPYdK%2FlpQF43w%3D%3D)
- **Terminals** [Phoenix Contact 1859991 (Same terminals as the 4 position plug)](https://www.mouser.com/ProductDetail/Phoenix-Contact/1859991?qs=Xflg4jNzfa6JFQqkZRuZkg%3D%3D)

## 3D Printed Hardware
- **Board Enclosure Body:**
    - **LIN Terminal Qty 3 Configuration:**
        - [STEP file](cad/step/board_enclosure_body_lin_terminal_qty_3.step)
        - [STL file](cad/stl/board_enclosure_body_lin_terminal_qty_3.stl)
        - [OnShape](https://cad.onshape.com/documents/1579fb2241bb92ec647abea8/w/72867be8bf1bda6b718069d5/e/ff063394a6becc890e2c6203?configuration=List_RQ8S9pCJ0h3FaR%3DDefault&renderMode=0&uiState=691a1a2a88d5628f75038e24)
    - **LIN Terminal Qty 4 Configuration:**
        - [STEP file](cad/step/board_enclosure_body_lin_terminal_qty_4.step)
        - [STL file](cad/stl/board_enclosure_body_lin_terminal_qty_4.stl)
        - [OnShape](https://cad.onshape.com/documents/1579fb2241bb92ec647abea8/w/72867be8bf1bda6b718069d5/e/ff063394a6becc890e2c6203?configuration=List_RQ8S9pCJ0h3FaR%3DLIN_Bus_Terminal_Quantity_4&renderMode=0&uiState=691a1a3d88d5628f75038e4f)
- **Board Enclosure Lid:**
    - [STEP file](cad/step/board_enclosure_lid.step)
    - [STL file](cad/stl/board_enclosure_lid.stl)
    - [OnShape](https://cad.onshape.com/documents/1579fb2241bb92ec647abea8/w/72867be8bf1bda6b718069d5/e/565005f997d62366ffdc6835?renderMode=0&uiState=691a19e376021baf6ef74d4d)
- **Button Module Mounting Flange (2 per Project):** 
    - [STEP file](cad/step/button_module_mounting_flange.step)
    - [STL file](cad/stl/button_module_mounting_flange.stl)
    - [OnShape](https://cad.onshape.com/documents/5132d3ba81357ba5c9de6664/w/01ddbd30e18f7d29c6d44511/e/b6893859f852cbca466a3d6f?renderMode=0&uiState=6918fc12eebe1b6cc4d0db89)
- **Button Module Mounting Base (2 per Project):** 
    - [STEP file](cad/step/button_module_mounting_base.step)
    - [STL file](cad/stl/button_module_mounting_base.stl)
    - [OnShape](https://cad.onshape.com/documents/5132d3ba81357ba5c9de6664/w/01ddbd30e18f7d29c6d44511/e/731e55fb501fd2d6602089eb?renderMode=0&uiState=6918fc17eebe1b6cc4d0db90)

**Note:** The recommended filament for these prints is ABS with 100% infill