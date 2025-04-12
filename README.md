# ESP32 Face Recognition Access Control System

A sophisticated access control system built with ESP32-CAM that uses facial recognition to authorize door access. The system operates in two modes - active and admin - and features WiFi configuration through a web interface.

## Overview

This project implements a face recognition-based access control system using the ESP32-CAM module. It includes:

- Real-time face detection and recognition
- Dual operation modes: active (normal operation) and admin (configuration)
- Secure door control using a relay
- Web-based administration interface
- WiFi access point configuration
- Face enrollment, recognition, and management

## Features

### Active Mode
- Monitors for faces using the camera
- Recognizes enrolled users and unlocks the door
- Automatically locks the door after a configurable interval
- Optimized for reliable operation

### Admin Mode
- Creates a WiFi access point for configuration
- Web interface for managing the system
- Face enrollment with multiple samples for improved recognition
- List and remove enrolled users
- Configure WiFi access point settings
- Test recognition and door control

### System Features
- Automatic mode switching between Active and Admin on reboot
- Persistent storage of settings and face data
- Secure operation with authenticated access

## Hardware Requirements

- ESP32-CAM module (AI-Thinker model recommended)
- Relay module for door control
- 5V power supply
- Door lock mechanism (electromagnetic or electric strike)
- Optional: housing and mounting hardware

## Software Components

### Main Program Structure

The codebase is organized into two main classes with specific responsibilities:

1. **ActiveMode Class**: Manages normal operation for access control
   - Performs continuous face detection and recognition
   - Controls door locking/unlocking
   - Optimized for reliability and performance

2. **AdminMode Class**: Provides configuration interface
   - Creates WiFi access point
   - Serves web interface
   - Processes configuration commands
   - Handles face enrollment and management
   - Configures network settings

### Key Components

- **Face Detection**: Uses MTCNN (Multi-task Cascaded Convolutional Networks) for reliable face detection
- **Face Recognition**: Implements face ID extraction and matching
- **Web Interface**: HTML/JavaScript interface for system administration
- **WebSockets**: Real-time communication between browser and device
- **Preferences**: Non-volatile storage for system settings

## Code Structure

### Core Files

- **Main Application**: Contains the setup, loop, and mode management
- **ActiveMode**: Implementation of the active mode functionality
- **AdminMode**: Implementation of the admin mode functionality
- **camera_index.h**: Web interface HTML and JavaScript

### Important Functions

#### Setup and Initialization
- `setup()`: Initializes hardware, loads settings, and starts the appropriate mode
- `ActiveMode::setup()` / `AdminMode::setup()`: Mode-specific initialization
- `app_facenet_main()`: Initializes face recognition components
- `app_mtmn_config()`: Configures face detection parameters

#### Face Processing
- `face_detect()`: Detects faces in camera frames
- `align_face()`: Aligns detected faces for recognition
- `get_face_id()`: Extracts face ID features
- `recognize_face_with_name()`: Matches face against enrolled faces
- `enroll_face_id_to_flash_with_name()`: Enrolls new faces

#### Web Interface
- `app_httpserver_init()`: Initializes HTTP server
- `index_handler()`: Serves the web interface
- `handle_message()`: Processes WebSocket messages
- `send_face_list()`: Sends list of enrolled faces to client
- `update_wifi_settings()`: Updates WiFi configuration

### Working Modes

#### Active Mode Operation
1. Camera continuously captures frames
2. Frames are processed for face detection
3. Detected faces are recognized against stored IDs
4. Recognized users trigger door unlock
5. Door automatically locks after timeout

#### Admin Mode Operation
1. Device creates WiFi access point
2. Web server provides administration interface
3. User connects to configure the system
4. WebSockets provide real-time camera feed and control
5. Face enrollment captures multiple samples for reliability

## Installation and Setup

### Building the Project

1. Install Arduino IDE and ESP32 board support
2. Install required libraries:
   - ESP32 Camera
   - ArduinoWebsockets
   - Preferences
3. Open the project in Arduino IDE
4. Select "ESP32 AI Thinker" as board type
5. Compile and upload the code

### Physical Setup

1. Connect the relay to GPIO pin 2
2. Connect the relay to your door lock mechanism
3. Power the ESP32-CAM with a stable 5V supply
4. Mount the camera with a clear view of the entry area

### First-Time Configuration

1. On first boot, the system will start in admin mode
2. Connect to the WiFi access point named "esp32" with password "12345678"
3. Open a browser and navigate to the IP address shown in the serial monitor
4. Enroll faces through the admin interface
5. Configure WiFi settings if needed
6. Restart the device to switch to active mode

### Using the Admin Interface

1. **Streaming**: View live camera feed
2. **Face Detection**: Test face detection
3. **Face Recognition**: Test recognition against enrolled faces
4. **Enroll**: Add new faces to the system
5. **WiFi Settings**: Configure access point SSID and password
6. **Remove Face**: Delete enrolled faces

## Customization

### Timeouts and Thresholds
- `interval`: Duration to keep door unlocked (default: 5000ms)
- Face detection thresholds in `app_mtmn_config()`

### Recognition Parameters
- `ENROLL_CONFIRM_TIMES`: Samples required for enrollment (default: 5)
- `FACE_ID_SAVE_NUMBER`: Maximum stored faces (default: 100)

### WiFi Configuration
- Default SSID and password can be changed in the code
- Can be configured through admin interface

## Troubleshooting

- **Camera Initialization Failure**: Check camera connections and power supply
- **Recognition Issues**: Improve lighting or adjust face detection parameters
- **Door Control Problems**: Verify relay connections and test manually
- **WiFi Connection Issues**: Check configured SSID and password

## Technical Notes

- The system automatically alternates between active and admin modes on reboot
- Face data is stored in flash memory and persists across reboots
- WiFi settings are stored in non-volatile memory
- The system uses model-based face recognition, not simple image matching

## Security Considerations

- Default WiFi credentials should be changed during setup
- Admin mode should be accessed only when configuration is needed
- Consider adding additional authentication for the admin interface
- Physical security of the device is important

## Advanced Usage

### API Reference

The WebSocket API supports the following commands:
- `stream`: Start streaming camera feed
- `detect`: Start face detection
- `recognise`: Start face recognition
- `capture:{name}`: Start enrollment of new face with specified name
- `remove:{name}`: Remove enrolled face with specified name
- `delete_all`: Delete all enrolled faces
- `wifi_settings`: Request current WiFi settings
- `update_wifi:{ssid}:{password}`: Update WiFi settings

## License

This project is released under the MIT License.
