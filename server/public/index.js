// Constants for message types and motor states
const MSG = {
    UNKNOWN: 0,
    MOTOR_STATE: 1,
    TELEMETRY: 2,
    CAMERA_CONTROL: 3,
    CAMERA_CHUNK: 4,
    MOTOR_CONTROL: 5,
    MOVE_CONTROL: 6,
    BATTERY_LEVEL: 7,
    DISTANCE_READING: 8
};

const MOTOR_STATE = {
    IDLE: 0,
    FORWARD: 1,
    BACKWARD: 2,
    BRAKE: 3
};

const MOVE_CMD = {
    M1_IDLE: 0,
    M1_FWD: 1,
    M1_BCK: 2,
    M1_BRK: 3,
    M2_IDLE: 4,
    M2_FWD: 5,
    M2_BCK: 6,
    M2_BRK: 7
};

// DOM Elements
const cameraFeed = document.getElementById('camera-feed');
const noCamera = document.getElementById('no-camera');
const connectionStatus = document.getElementById('connection-status');
const motor1Status = document.getElementById('motor1-status');
const motor2Status = document.getElementById('motor2-status');
const batteryFill = document.querySelector('.battery-fill');
const batteryText = document.querySelector('.battery-text');
const distanceText = document.querySelector('.distance-text');
const brakeBtn = document.getElementById('brake-btn');
const motorButtons = document.querySelectorAll('.control-btn');

// WebSocket connection
const ws = new WebSocket((location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws');
ws.binaryType = 'arraybuffer';

// Current motor states
let currentMotorStates = {
    motor1: MOTOR_STATE.IDLE,
    motor2: MOTOR_STATE.IDLE
};

// Connection status handling
ws.onopen = function() {
    connectionStatus.textContent = 'Connected';
    connectionStatus.classList.add('connected');
};

ws.onclose = function() {
    connectionStatus.textContent = 'Disconnected';
    connectionStatus.classList.remove('connected');
};

ws.onerror = function() {
    connectionStatus.textContent = 'Connection Error';
    connectionStatus.classList.remove('connected');
};

// Helper function to create binary messages
function createBinaryMessage(msgId, payload) {
    const buffer = new ArrayBuffer(3 + payload.length);
    const view = new Uint8Array(buffer);
    view[0] = msgId;
    view[1] = payload.length & 0xFF;
    view[2] = (payload.length >> 8) & 0xFF;
    for (let i = 0; i < payload.length; i++) {
        view[3 + i] = payload[i];
    }
    return buffer;
}

// Helper function to send motor control commands
function sendMotorControl(motor1State, motor2State) {
    const payload = new Uint8Array([motor1State, motor2State]);
    const message = createBinaryMessage(MSG.MOTOR_CONTROL, payload);
    ws.send(message);
    updateMotorStatus(motor1State, motor2State);
}

// Helper function to send move control commands
function sendMoveControl(moveCmd) {
    const payload = new Uint8Array([moveCmd]);
    const message = createBinaryMessage(MSG.MOVE_CONTROL, payload);
    ws.send(message);
    
    // Update UI based on move command
    switch(moveCmd) {
        case MOVE_CMD.M1_FWD:
            updateMotorStatus(MOTOR_STATE.FORWARD, currentMotorStates.motor2);
            break;
        case MOVE_CMD.M1_BCK:
            updateMotorStatus(MOTOR_STATE.BACKWARD, currentMotorStates.motor2);
            break;
        case MOVE_CMD.M1_BRK:
            updateMotorStatus(MOTOR_STATE.BRAKE, currentMotorStates.motor2);
            break;
        case MOVE_CMD.M1_IDLE:
            updateMotorStatus(MOTOR_STATE.IDLE, currentMotorStates.motor2);
            break;
        case MOVE_CMD.M2_FWD:
            updateMotorStatus(currentMotorStates.motor1, MOTOR_STATE.FORWARD);
            break;
        case MOVE_CMD.M2_BCK:
            updateMotorStatus(currentMotorStates.motor1, MOTOR_STATE.BACKWARD);
            break;
        case MOVE_CMD.M2_BRK:
            updateMotorStatus(currentMotorStates.motor1, MOTOR_STATE.BRAKE);
            break;
        case MOVE_CMD.M2_IDLE:
            updateMotorStatus(currentMotorStates.motor1, MOTOR_STATE.IDLE);
            break;
    }
}

// Update motor status display
function updateMotorStatus(motor1State, motor2State) {
    currentMotorStates.motor1 = motor1State;
    currentMotorStates.motor2 = motor2State;
    
    const stateNames = ['Idle', 'Forward', 'Backward', 'Brake'];
    
    motor1Status.textContent = `M1: ${stateNames[motor1State]}`;
    motor2Status.textContent = `M2: ${stateNames[motor2State]}`;
    
    // Update motor status styling
    motor1Status.className = 'motor-status';
    motor2Status.className = 'motor-status';
    
    if (motor1State === MOTOR_STATE.FORWARD) {
        motor1Status.classList.add('forward');
    } else if (motor1State === MOTOR_STATE.BACKWARD) {
        motor1Status.classList.add('backward');
    } else if (motor1State === MOTOR_STATE.BRAKE) {
        motor1Status.classList.add('brake');
    }
    
    if (motor2State === MOTOR_STATE.FORWARD) {
        motor2Status.classList.add('forward');
    } else if (motor2State === MOTOR_STATE.BACKWARD) {
        motor2Status.classList.add('backward');
    } else if (motor2State === MOTOR_STATE.BRAKE) {
        motor2Status.classList.add('brake');
    }
}

// Update battery level display
function updateBatteryLevel(level) {
    const percentage = Math.round((level / 255) * 100);
    batteryFill.style.width = `${percentage}%`;
    batteryText.textContent = `${percentage}%`;
    
    // Update color based on level
    if (percentage > 60) {
        batteryFill.style.backgroundColor = '#28a745'; // Green
    } else if (percentage > 20) {
        batteryFill.style.backgroundColor = '#ffc107'; // Yellow
    } else {
        batteryFill.style.backgroundColor = '#dc3545'; // Red
    }
}

// Update distance display
function updateDistance(distance) {
    distanceText.textContent = `${distance} cm`;
}

// WebSocket message handling
ws.onmessage = function(event) {
    if (event.data instanceof ArrayBuffer) {
        const data = new Uint8Array(event.data);
        
        // Check if we have at least a header
        if (data.length < 3) {
            console.error('Received message too short');
            return;
        }
        
        const msgId = data[0];
        const length = data[1] | (data[2] << 8);
        const payload = data.subarray(3);
        
        if (payload.length !== length) {
            console.error(`Payload length mismatch: expected ${length}, got ${payload.length}`);
            return;
        }
        
        switch (msgId) {
            case MSG.TELEMETRY:
                if (payload.length === 4) {
                    const telemetry = {
                        motor1: payload[0],
                        motor2: payload[1],
                        battery: payload[2],
                        distance: payload[3]
                    };
                    
                    updateMotorStatus(telemetry.motor1, telemetry.motor2);
                    updateBatteryLevel(telemetry.battery);
                    updateDistance(telemetry.distance);
                }
                break;
                
            case MSG.CAMERA_CHUNK:
                // Hide the "No Camera" message when we receive camera data
                noCamera.style.display = 'none';
                
                // Create a blob from the image data
                const blob = new Blob([payload], { type: 'image/jpeg' });
                const url = URL.createObjectURL(blob);
                cameraFeed.src = url;
                cameraFeed.onload = () => URL.revokeObjectURL(url);
                break;
                
            case MSG.BATTERY_LEVEL:
                if (payload.length === 1) {
                    updateBatteryLevel(payload[0]);
                }
                break;
                
            case MSG.DISTANCE_READING:
                if (payload.length === 1) {
                    updateDistance(payload[0]);
                }
                break;
        }
    } else {
        console.log(`WS text: ${event.data}`);
    }
};

// Keyboard controls for desktop
document.addEventListener('keydown', function(event) {
    // Prevent default behavior for these keys
    if (['q', 'w', 'e', 'a', 's', 'd', ' '].includes(event.key.toLowerCase())) {
        event.preventDefault();
    }
    
    switch (event.key.toLowerCase()) {
        case 'q': // M1 Forward
            sendMoveControl(MOVE_CMD.M1_FWD);
            break;
        case 'w': // M1+M2 Forward
            sendMotorControl(MOTOR_STATE.FORWARD, MOTOR_STATE.FORWARD);
            break;
        case 'e': // M2 Forward
            sendMoveControl(MOVE_CMD.M2_FWD);
            break;
        case 'a': // M1 Backward
            sendMoveControl(MOVE_CMD.M1_BCK);
            break;
        case 's': // M1+M2 Backward
            sendMotorControl(MOTOR_STATE.BACKWARD, MOTOR_STATE.BACKWARD);
            break;
        case 'd': // M2 Backward
            sendMoveControl(MOVE_CMD.M2_BCK);
            break;
        case ' ': // Brake
            sendMotorControl(MOTOR_STATE.BRAKE, MOTOR_STATE.BRAKE);
            break;
    }
});

// Release keys to stop motors
document.addEventListener('keyup', function(event) {
    switch (event.key.toLowerCase()) {
        case 'q': // Stop M1 if it was going forward
            if (currentMotorStates.motor1 === MOTOR_STATE.FORWARD) {
                sendMoveControl(MOVE_CMD.M1_IDLE);
            }
            break;
        case 'e': // Stop M2 if it was going forward
            if (currentMotorStates.motor2 === MOTOR_STATE.FORWARD) {
                sendMoveControl(MOVE_CMD.M2_IDLE);
            }
            break;
        case 'a': // Stop M1 if it was going backward
            if (currentMotorStates.motor1 === MOTOR_STATE.BACKWARD) {
                sendMoveControl(MOVE_CMD.M1_IDLE);
            }
            break;
        case 'd': // Stop M2 if it was going backward
            if (currentMotorStates.motor2 === MOTOR_STATE.BACKWARD) {
                sendMoveControl(MOVE_CMD.M2_IDLE);
            }
            break;
        case 'w': // Stop both motors if they were going forward
        case 's': // Stop both motors if they were going backward
            if ((event.key.toLowerCase() === 'w' && 
                 currentMotorStates.motor1 === MOTOR_STATE.FORWARD && 
                 currentMotorStates.motor2 === MOTOR_STATE.FORWARD) ||
                (event.key.toLowerCase() === 's' && 
                 currentMotorStates.motor1 === MOTOR_STATE.BACKWARD && 
                 currentMotorStates.motor2 === MOTOR_STATE.BACKWARD)) {
                sendMotorControl(MOTOR_STATE.IDLE, MOTOR_STATE.IDLE);
            }
            break;
    }
});

// Touch controls for mobile
motorButtons.forEach(button => {
    // Handle touch start/mouse down
    ['touchstart', 'mousedown'].forEach(eventType => {
        button.addEventListener(eventType, function(e) {
            e.preventDefault();
            const motor = parseInt(this.dataset.motor);
            const direction = this.dataset.direction;
            
            if (motor === 1) {
                if (direction === 'forward') {
                    sendMoveControl(MOVE_CMD.M1_FWD);
                } else {
                    sendMoveControl(MOVE_CMD.M1_BCK);
                }
            } else {
                if (direction === 'forward') {
                    sendMoveControl(MOVE_CMD.M2_FWD);
                } else {
                    sendMoveControl(MOVE_CMD.M2_BCK);
                }
            }
        });
    });
    
    // Handle touch end/mouse up
    ['touchend', 'mouseup', 'mouseleave'].forEach(eventType => {
        button.addEventListener(eventType, function(e) {
            e.preventDefault();
            const motor = parseInt(this.dataset.motor);
            
            if (motor === 1) {
                sendMoveControl(MOVE_CMD.M1_IDLE);
            } else {
                sendMoveControl(MOVE_CMD.M2_IDLE);
            }
        });
    });
});

// Brake button
['touchstart', 'mousedown'].forEach(eventType => {
    brakeBtn.addEventListener(eventType, function(e) {
        e.preventDefault();
        sendMotorControl(MOTOR_STATE.BRAKE, MOTOR_STATE.BRAKE);
    });
});

['touchend', 'mouseup', 'mouseleave'].forEach(eventType => {
    brakeBtn.addEventListener(eventType, function(e) {
        e.preventDefault();
        sendMotorControl(MOTOR_STATE.IDLE, MOTOR_STATE.IDLE);
    });
});

// Add CSS classes for motor states
document.head.insertAdjacentHTML('beforeend', `
<style>
.motor-status.forward {
    background-color: #d4edda;
    color: #155724;
}
.motor-status.backward {
    background-color: #f8d7da;
    color: #721c24;
}
.motor-status.brake {
    background-color: #fff3cd;
    color: #856404;
}
</style>
`);

// Check if we're on a mobile device
function isMobileDevice() {
    return (window.innerWidth <= 767);
}

// Show/hide appropriate controls based on device
function updateControlsVisibility() {
    const controlInfo = document.querySelector('.control-info');
    const mobileControls = document.getElementById('mobile-controls');
    
    if (isMobileDevice()) {
        controlInfo.style.display = 'none';
        mobileControls.style.display = 'grid';
    } else {
        controlInfo.style.display = 'block';
        mobileControls.style.display = 'none';
    }
}

// Update controls visibility on load and resize
window.addEventListener('load', updateControlsVisibility);
window.addEventListener('resize', updateControlsVisibility);
