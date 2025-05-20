// nodejs websocket server, without any external libraries
// the server will server a static filder from public/
// the server will also serve a websocket connection from /ws
// the server will also serve a websocket connection from /wsc
// sensible values should be configurable from env

import { createServer } from 'http';
import { createReadStream } from 'fs';
import { WebSocketServer } from 'ws';
import { EventEmitter } from 'events';

const throttle = (func, delay) => {
    let lastCall = 0;
    return function (...args) {
        const now = Date.now();
        if (now - lastCall < delay) {
            return;
        }
        lastCall = now;
        return func.apply(this, args);
    };
}

const debounce = (func, delay) => {
    let timeout;
    return function (...args) {
        const context = this;
        clearTimeout(timeout);
        timeout = setTimeout(() => func.apply(context, args), delay);
    };
}

// --- Message protocol constants and helpers (from messages.h) ---
const MSG_ID = {
    UNKNOWN: 0,
    MOTOR_STATE: 1,
    TELEMETRY: 2,
    CAMERA_CONTROL: 3,
    CAMERA_CHUNK: 4,
    MOTOR_CONTROL: 5,
    MOVE_CONTROL: 6,
    BATTERY_LEVEL: 7,
    DISTANCE_READING: 8,
};

const MSG_MOTOR_STATE = {
    IDLE: 0,
    FORWARD: 1,
    BACKWARD: 2,
    BRAKE: 3,
};

const MSG_MOVE_CMD = {
    M1_IDLE: 0,
    M1_FWD: 1,
    M1_BCK: 2,
    M1_BRK: 3,
    M2_IDLE: 4,
    M2_FWD: 5,
    M2_BCK: 6,
    M2_BRK: 7,
};

class BaseWS extends WebSocketServer {

    constructor(eventBus, options = {}) {
        options.noServer = true;
        super(options);
        this.eventBus = eventBus;
        this.clients = new Set(); // Track connected clients
        this.on('connection', (ws, req) => {
            this.clients.add(ws); // Add client
            ws.on('message', this.onMessage.bind(this));
            ws.on('error', this.onError.bind(this));
            ws.on('close', () => {
                this.clients.delete(ws); // Remove client on close
                this.onDisconnect(ws);
                this.onClose();
            });
            this.onConnection(ws, req);
        });
    }

    handleUpgrade(req, socket, head, callback = null) {
        super.handleUpgrade(req, socket, head, (ws) => {
            this.emit('connection', ws, req);
            if (callback) {
                callback(ws, req);
            }
        });
    }

    onConnection(ws, req) {
        console.log('WS client connected:', ws._socket.remoteAddress);
    }
    onDisconnect(ws) {
        console.log('WS client disconnected:', ws._socket.remoteAddress);
    }
    onMessage(message, isBinary) {
        if (isBinary) {
            console.log('WS binary length:', message.length);
        }
        else {
            console.log(`WS text: ${message}`);
        }
    }
    onError(error) {
        console.error(`WS error: ${error}`);
    }
    onClose() {
        console.log('WS closed');
    }

    broadcast(data, binary = true) {
        this.clients.forEach((client) => {
            if (client.readyState === client.OPEN) {
                client.send(data, { binary });
            }
        });
    }
}

class ControlWS extends BaseWS {
    onConnection(ws, req) {
        console.log('ORE0 connected:', ws._socket.remoteAddress);
    }
    onDisconnect(ws) {
        console.log('ORE0 disconnected:', ws._socket.remoteAddress);
    }
    onMessage(message, isBinary) {
        if (isBinary) {
            // Parse message according to messages.h
            if (message.length < 3) {
                console.error('WS binary too short for header');
                return;
            }
            const msg_id = message[0];
            const length = message[1] | (message[2] << 8);
            const payload = message.subarray(3);
            if (payload.length !== length) {
                console.error(`WS payload length mismatch: expected ${length}, got ${payload.length}`);
                return;
            }
            switch (msg_id) {
                case MSG_ID.TELEMETRY:
                    // struct: uint8 m1, uint8 m2, uint8 battery, uint8 distance
                    if (payload.length !== 4) {
                        console.error('Telemetry payload length invalid');
                        return;
                    }
                    const telemetry = {
                        motor1: payload[0],
                        motor2: payload[1],
                        battery: payload[2],
                        distance: payload[3],
                    };
                    console.log('Telemetry:', telemetry);
                    this.eventBus.emit('telemetry', telemetry);
                    break;
                // case MSG_ID.CAMERA_CONTROL:
                //     // struct: uint8 on
                //     if (payload.length !== 1) {
                //         console.error('Camera control payload length invalid');
                //         return; 
                //     }
                //     const camctrl = { on: payload[0] };
                //     console.log('Camera control:', camctrl);
                //     this.eventBus.emit('camera_control', camctrl);
                //     break;
                case MSG_ID.CAMERA_CHUNK:
                    // payload: image data only, length = payload.length
                    // No chunk_len field, just the image data
                    if (payload.length < 1) {
                        console.error('Camera chunk payload too short');
                        return;
                    }
                    // Forward the raw image data to clients
                    this.eventBus.emit('image', message);
                    break;
                case MSG_ID.MOTOR_CONTROL:
                    // struct: uint8 m1, uint8 m2
                    if (payload.length !== 2) {
                        console.error('Motor control payload length invalid');
                        return;
                    }
                    const mctrl = {
                        motor1: payload[0],
                        motor2: payload[1],
                    };
                    console.log('Motor control:', mctrl);
                    this.eventBus.emit('motor_control', mctrl);
                    break;
                case MSG_ID.BATTERY_LEVEL:
                    // struct: uint8 level
                    if (payload.length !== 1) {
                        console.error('Battery level payload length invalid, got', payload.length);
                        return;
                    }
                    const battery = payload[0];
                    console.log('Battery level:', battery);
                    this.eventBus.emit('battery_level', { battery });
                    break;
                case MSG_ID.DISTANCE_READING:
                    // struct: uint8 distance
                    if (payload.length !== 1) {
                        console.error('Distance reading payload length invalid, got', payload.length);
                        return;
                    }
                    const distance = payload[0];
                    console.log('Distance reading:', distance);
                    this.eventBus.emit('distance_reading', { distance });
                    break;
                default:
                    console.error('WS unknown binary msg_id:', msg_id, 'length:', length, 'payload.length:', payload.length, 'payload:', payload);
            }
        } else {
            console.log(`WS text: ${message}`);
        }
    }
}

class ClientWS extends BaseWS {
    onConnection(ws, req) {
        super.onConnection(ws, req);
        this.eventBus.emit('client_count', this.clients.size);
        // Notify ORE0 to turn camera on if this is the first client
        if (this.clients.size === 1) {
            // MSG_CAMERA_CONTROL: id=3, len=1, payload=1 (on)
            const buf = Buffer.from([3, 1, 0, 1]);
            this.eventBus.emit('camera_control_request', buf);
        }
    }
    onDisconnect(ws) {
        this.eventBus.emit('client_count', this.clients.size);
        super.onDisconnect(ws);
        // Notify ORE0 to turn camera off if this was the last client
        if (this.clients.size === 0) {
            // MSG_CAMERA_CONTROL: id=3, len=1, payload=0 (off)
            const buf = Buffer.from([3, 1, 0, 0]);
            this.eventBus.emit('camera_control_request', buf);
        }
    }
    onMessage(message, isBinary) {
        // Forward move/motor control commands to ORE0 (wsc)
        if (isBinary && message.length >= 3) {
            const msg_id = message[0];
            if (msg_id === 5 || msg_id === 6) { // MSG_MOTOR_CONTROL or MSG_MOVE_CONTROL
                this.eventBus.emit('client_command', message);
                return;
            }
        }
        super.onMessage(message, isBinary);
    }
}



const wsBus = new EventEmitter();
const ws = new ClientWS(wsBus);
const wsc = new ControlWS(wsBus);



// Event bus logic for camera control and telemetry forwarding
wsBus.on('camera_control_request', (buf) => {
    wsc.broadcast(buf);
});

wsBus.on('telemetry', (telemetry) => {
    // Construct telemetry message: id=2, len=4, payload
    const buf = Buffer.from([2, 4, 0, telemetry.motor1, telemetry.motor2, telemetry.battery, telemetry.distance]);
    ws.broadcast(buf);
});

wsBus.on('motor_control', (mctrl) => {
    // Forward motor control state to ws clients if needed (optional)
});

wsBus.on('client_command', (buf) => {
    // Forward client move/motor control commands to ORE0
    wsc.broadcast(buf);
});

wsBus.on('image', (image) => {
    throttle(() => console.log('Image:', image.length), 1000)();
    ws.broadcast(image);
});



function makeTelemetry(m1, m2, battery, distance) {
    // msg_id, len=4, payload
    return Buffer.from([
        MSG_ID.TELEMETRY, 4, 0, m1, m2, battery, distance
    ]);
}

function makeCameraControl(on) {
    // msg_id, len=1, payload
    return Buffer.from([
        MSG_ID.CAMERA_CONTROL, 1, 0, on ? 1 : 0
    ]);
}

function makeMotorControl(m1, m2) {
    // msg_id, len=2, payload
    return Buffer.from([
        MSG_ID.MOTOR_CONTROL, 2, 0, m1, m2
    ]);
}

function makeMoveControl(cmd) {
    // msg_id, len=1, payload
    return Buffer.from([
        MSG_ID.MOVE_CONTROL, 1, 0, cmd
    ]);
}

function makeCameraChunk(jpegBuffer) {
    // msg_id, len=2+N, payload: chunk_len (LE), data[]
    const len = jpegBuffer.length;
    const header = Buffer.from([
        MSG_ID.CAMERA_CHUNK,
        (len + 2) & 0xFF, ((len + 2) >> 8) & 0xFF,
        len & 0xFF, (len >> 8) & 0xFF
    ]);
    return Buffer.concat([header, jpegBuffer]);
}

function parseTelemetry(payload) {
    if (payload.length !== 4) return null;
    return {
        motor1: payload[0],
        motor2: payload[1],
        battery: payload[2],
        distance: payload[3],
    };
}

function parseCameraControl(payload) {
    if (payload.length !== 1) return null;
    return { on: payload[0] };
}

function parseMotorControl(payload) {
    if (payload.length !== 2) return null;
    return {
        motor1: payload[0],
        motor2: payload[1],
    };
}

function parseMoveControl(payload) {
    if (payload.length !== 1) return null;
    return { cmd: payload[0] };
}

function parseCameraChunk(payload) {
    if (payload.length < 2) return null;
    const chunk_len = payload[0] | (payload[1] << 8);
    const data = payload.subarray(2);
    if (data.length !== chunk_len) return null;
    return { chunk_len, data };
}


// create http server
const server = createServer((req, res) => {
    // serve static files from public/ under index. respect content-type
    // if the request is for /, serve index.html
    if (req.url === '/') {
        req.url = '/index.html';
    }
    const filePath = `public${req.url}`;
    const fileStream = createReadStream(filePath);
    fileStream.on('open', () => {
        // handle mime types
        const mimeTypes = {
            '.html': 'text/html',
            '.js': 'application/javascript',
            '.css': 'text/css',
            '.png': 'image/png',
            '.jpg': 'image/jpeg',
            '.jpeg': 'image/jpeg',
            '.gif': 'image/gif',
            '.svg': 'image/svg+xml',
            '.json': 'application/json',
        };
        const ext = filePath.split('.').pop();
        const contentType = mimeTypes[`.${ext}`] || 'application/octet-stream';
        res.writeHead(200, { 'Content-Type': contentType });
        fileStream.pipe(res);
    });
    fileStream.on('error', (err) => {
        console.error(err);
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('404 Not Found');
    });
    // handle 404 for other requests
});

// handle websocket upgrade requests
server.on('upgrade', (req, socket, head) => {
    if (req.url === '/ws') {
        ws.handleUpgrade(req, socket, head);
    } else if (req.url === '/wsc') {
        wsc.handleUpgrade(req, socket, head);
    } else {
        socket.destroy();
    }
});


// start server
const PORT = process.env.PORT || 36080;
const HOST = process.env.HOST || '0.0.0.0';
// do not listen on ipv6
server.listen(PORT, HOST, () => {
    const host = HOST === '0.0.0.0' ? 'localhost' : HOST;
    const port = PORT === 80 ? '' : `:${PORT}`;
    console.log(`Server is listening on http://${host}${port}/`);
});
