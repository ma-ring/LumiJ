# LumiJ - LED Controller System

A multi-microcontroller LED control system with BPM synchronization and touch interface.

## Project Structure

```
LumiJ/
├── shared/                    # Shared constants and definitions
│   └── const.h               # Master constants file
├── Dial/                     # M5Dial controller
│   ├── Dial.ino             # Main firmware
│   ├── const.h              # Auto-copied from shared/
│   └── dial_upload.bat      # Upload script with auto-copy
├── Stamp1/                   # Key matrix controller
│   ├── Stamp1.ino           # Key matrix firmware
│   ├── const.h              # Auto-copied from shared/
│   └── stamp_upload.bat     # Upload script with auto-copy
└── Stamp2/                   # LED controller
    ├── Stamp2.ino           # LED control firmware
    ├── const.h              # Auto-copied from shared/
    └── stamp_upload.bat     # Upload script with auto-copy
```

## Hardware Components

- **Dial**: M5Dial with touch interface and rotary encoder
- **Stamp1**: M5StampS3 with 4x4 key matrix
- **Stamp2**: M5StampS3 with 16-LED shift register control

## Features

- **BPM Synchronization**: Tap-based BPM detection with fine-tuning
- **Wave Patterns**: Square, Triangle, Sine, Saw wave patterns for LEDs
- **Touch Interface**: Multi-zone touch control for mode switching
- **UART Communication**: Reliable communication between microcontrollers
- **Error Handling**: Communication timeout detection and recovery

## Development Workflow

### Constants Management
All shared constants are managed in `shared/const.h`. Upload scripts automatically copy the latest version to each microcontroller directory before compilation.

### Building and Uploading
```bash
# Upload to Dial (COM7)
cd Dial && .\dial_upload.bat COM7

# Upload to Stamp1 (COM8)  
cd Stamp1 && .\stamp_upload.bat COM8

# Upload to Stamp2 (COM9)
cd Stamp2 && .\stamp_upload.bat COM9
```

### Dependencies
- Arduino IDE or Arduino CLI
- M5Stack board packages
- ESP32 core for Arduino

## Communication Protocol

### Dial → Stamp1
- `KEY_DOWN:id` - Key press notification
- `KEY_BUTTON:PRESSED` - Mode switch notification

### Dial → Stamp2  
- `SET_BEAT:id,beat` - LED beat pattern setting
- `SET_WAVE:id,wave` - LED wave pattern setting
- `SET_BPM:bpm` - Global BPM setting

## Testing and CI/CD

- **GitHub Actions**: Automated compilation checks
- **Unit Tests**: BPM calculation and communication protocol tests
- **Integration Tests**: End-to-end system validation

## License

[Add your license here]