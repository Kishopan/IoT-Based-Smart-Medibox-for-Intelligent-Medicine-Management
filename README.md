# IoT-Based Smart Medibox

An **IoT-enabled Smart Medibox** simulated on Wokwi using ESP32. The system ensures **timely medicine reminders**, monitors **environmental conditions** for safe storage, and allows **remote monitoring and control** via Node-RED.

## Features

- **Medicine Reminder System**: Alarm scheduling with NTP-based time synchronization and OLED display notifications.
- **Environmental Monitoring**: Temperature and humidity sensors trigger buzzer/LED alerts for unsafe storage conditions.
- **Real-Time Dashboard**: Integrated Node-RED dashboard for live monitoring and remote configuration.
- **Light Intensity Monitoring**: LDR-based dynamic light measurement with configurable sampling and data upload intervals.
- **Servo-Controlled Storage Optimization**: Automatic adjustment of a shaded sliding window based on light and temperature for optimal medicine storage.

## Hardware & Simulation

- **Microcontroller**: ESP32
- **Sensors**: Temperature, Humidity, LDR
- **Actuators**: Servo motor for sliding window
- **Simulator**: Wokwi Arduino Simulator
- **Display**: OLED (for reminders and alerts)
- **IoT Platform**: Node-RED dashboard

