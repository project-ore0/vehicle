# Protobuf integration for ore0-mark3

This directory contains the Protocol Buffers schema for all messages exchanged in the project.

- `messages.proto` is the source of truth for all message formats.

## Usage

1. Edit `messages.proto` to update the message schema.
2. Use the protobuf compiler (`protoc`) to generate C source/header files for use in the ESP-IDF project.
3. The project uses the `esp_google_protobuf` component for protobuf support on ESP32.

## Directory structure

- `proto/messages.proto` - Protocol Buffers schema
- `proto/README.md` - This file
