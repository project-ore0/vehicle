# ORE0 Control Panel

This project provides a modern TypeScript-based implementation for the ORE0 control panel, consisting of a NestJS backend and a React frontend.

## Project Structure

```
server/
├── nestjs-server/           # Backend (NestJS)
│   ├── src/
│   │   ├── main.ts          # Entry point
│   │   ├── app.module.ts    # Main module
│   │   ├── messages/        # Message protocol definitions
│   │   └── websockets/      # WebSocket gateways
│   │       ├── client.gateway.ts  # /ws endpoint
│   │       ├── control.gateway.ts # /wsc endpoint
│   │       ├── event-bus.service.ts # Communication between gateways
│   │       └── websocket.module.ts  # WebSocket module
│   ├── public/              # Static files (will be served by NestJS)
│   ├── package.json         # Backend dependencies
│   └── tsconfig.json        # TypeScript configuration
│
├── react-client/            # Frontend (React + TypeScript)
│   ├── src/
│   │   ├── App.tsx          # Main App component
│   │   ├── index.tsx        # Entry point
│   │   ├── components/      # React components
│   │   │   ├── CameraFeed.tsx
│   │   │   ├── ControlPanel.tsx
│   │   │   └── VehicleStatus.tsx
│   │   ├── hooks/           # Custom React hooks
│   │   │   └── useWebSocket.ts
│   │   └── utils/           # Utility functions
│   │       └── message-protocol.ts
│   ├── package.json         # Frontend dependencies
│   └── tsconfig.json        # TypeScript configuration
│
├── server.js                # Original Node.js server (for reference)
└── public/                  # Original static files (for reference)
```

## Technologies Used

### Backend
- **NestJS**: A progressive Node.js framework for building efficient and scalable server-side applications.
- **WebSockets**: For real-time communication between the server and clients.
- **TypeScript**: For type safety and better developer experience.

### Frontend
- **React**: A JavaScript library for building user interfaces.
- **TypeScript**: For type safety and better developer experience.
- **CSS**: For styling the components.

## Getting Started

### Backend Setup
1. Navigate to the backend directory:
   ```
   cd nestjs-server
   ```
2. Install dependencies:
   ```
   npm install
   ```
3. Start the development server:
   ```
   npm run start:dev
   ```

### Frontend Setup
1. Navigate to the frontend directory:
   ```
   cd react-client
   ```
2. Install dependencies:
   ```
   npm install
   ```
3. Start the development server:
   ```
   npm start
   ```

## WebSocket Endpoints

- **/ws**: For client connections (browser)
- **/wsc**: For control connections (ORE0 device)

## Message Protocol

The message protocol is defined in TypeScript and follows the same structure as the original C implementation in `messages.h`. The protocol includes message types for:

- Motor state
- Telemetry
- Camera control
- Camera chunks
- Motor control
- Move control
- Battery level
- Distance reading

## Features

- Real-time camera feed display
- Motor control via keyboard and touch/mouse
- Battery level and distance display
- Responsive design for both desktop and mobile devices
