# Software Development Methodology

## Development Process

This project involved:
1. **Reverse Engineering:** Capturing and analyzing LIN and CAN bus traffic
2. **Protocol Documentation:** Mapping message structures for both buses
3. **Test Development:** Validating LIN control and button detection
4. **Gateway Implementation:** Building bidirectional protocol translation

See the [`docs/CAN_analysis/`](docs/CAN_analysis/) and [`docs/LIN_analysis/`](docs/LIN_analysis/) directories for detailed analysis workflows and results.

## Reverse Engineering Overview
The steps taken for identifying the relevant messages in both protocols were as follows:
1. **Capture Data**
    - Passively monitor bus traffic through a variety of scenarios, including baselines.
        - Scenarios for LIN included:
            - Both seat button modules disconnected from the bus
                - No interaction (baseline)
                - Starting with the lights off, waiting **3** seconds, cycling them on, waiting for **3** seconds, cycling them off, and repeating
            - Driver's seat button module connected to the bus
                - No interaction (baseline)
                - Press the **Heat** button every **3** seconds, cycling from Off->High->Medium->Low->Off
                - Press the **Vent** button every **3** seconds, cycling from Off->High->Medium->Low->Off
                - Starting with the lights off, waiting **3** seconds, cycling them on, waiting for **3** seconds, cycling them off, and repeating
            - Passengers's seat button module connected to the bus
                - No interaction (baseline)
                - Press the **Heat** button every **3** seconds, cycling from Off->High->Medium->Low->Off
                - Press the **Vent** button every **3** seconds, cycling from Off->High->Medium->Low->Off
                - Starting with the lights off, waiting **3** seconds, cycling them on, waiting for **3** seconds, cycling them off, and repeating
        - Scenarios for CAN included:
            - No interaction (baseline)
            - Starting with the lights off, waiting **3** seconds, cycling them on, waiting for **3** seconds, cycling them off, and repeating
            - Starting with the interior light dimmer at it's brightest setting, waiting **3** seconds, changing to the dimmest setting, waiting for **3** seconds, changing to the brighest setting, and repeating
            - Press the **Driver's Heat** button every **3** seconds, cycling from Off->High->Medium->Low->Off
            - Press the **Driver's Vent** button every **3** seconds, cycling from Off->High->Medium->Low->Off 
            - Press the **Passenger's Heat** button every **3** seconds, cycling from Off->High->Medium->Low->Off
            - Press the **Passenger's Vent** button every **3** seconds, cycling from Off->High->Medium->Low->Off 

2. **Transform Data**
    - In the case of the LIN bus captures, they had to first be parsed from raw UART data into LIN messages
    - Group unique messages together, with counts of occurrences during the capture period
    - Compare unique messages between interactive scenarios and baseline scenarios, identifying what messages were unqie to each capture in a comparison
3. **Analyze Data**
    - Look for messages, that were present in the interactive scenarios and not the baseline scenarios, that align with the cadence of the interactions

## [Testing](src/test/README.md)

The `src/test/` directory contains subdirectores for validation sketches:
- **backlighting_sequence.ino:** Discovery tool to reverse engineer backlighting commands
- **buttons.ino:** Basic button press detection and LED control
- **buttons_with_backlighting.ino:** Complete LIN master implementation combining all functionality
- **backlighting_toggle.ino:** Backlighting control with brightness adjustment

