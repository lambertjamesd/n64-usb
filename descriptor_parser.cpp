
#include "descriptor_parser.h"

#include <string.h>
#include <stddef.h>

#include <Arduino.h>

enum ConfigParserState {
    ConfigParserStateFindingInterfaceClass,
    ConfigParserStateFindingInterfaceSubClass,
    ConfigParserStateFindingInterfaceProtocol,

    ConfigParserStateFindingEndpoint,

    ConfigParserStateDone,
};

struct ConfigParser {
    uint8_t state;
    uint8_t descSize;
    uint8_t descType;
    uint8_t descOffset;

    struct HidInfo* info;
};

void configParserInit(struct ConfigParser* parser, struct HidInfo* info) {
    parser->state = ConfigParserStateFindingInterfaceClass;
    parser->descSize = 0;
    parser->descType = 0;
    // size of header
    parser->descOffset = 2;
    parser->info = info;

    info->bootMouseEndpoint = 0;
}

void configParserStep(struct ConfigParser* parser, uint8_t next) {
    if (parser->descSize == 0) {
        parser->descSize = next;
        Serial.print("descSize ");
        Serial.print(next);
        Serial.print("\n");
        return;
    }

    if (parser->descType == 0) {
        parser->descType = next;
        Serial.print("descType ");
        Serial.print(next);
        Serial.print("\n");
        return;
    }

    switch (parser->state) {
        case ConfigParserStateFindingInterfaceClass:
            if (parser->descType == DESC_TYPE_INTERFACE && 
                parser->descOffset == offsetof(struct InterfaceDescriptor, bInterfaceNumber)) {
                parser->info->bootMouseInterface = next;

                Serial.print("Using interface");
                Serial.print(next);
                Serial.print("\n");
            } else if (parser->descType == DESC_TYPE_INTERFACE && 
                parser->descOffset == offsetof(struct InterfaceDescriptor, bInterfaceClass) &&
                next == DEVICE_CLASS_HID) {
                parser->state = ConfigParserStateFindingInterfaceSubClass;
                Serial.print("Correct class\n");
            }
            break;
        case ConfigParserStateFindingInterfaceSubClass:
            if (parser->descType == DESC_TYPE_INTERFACE &&
                parser->descOffset == offsetof(struct InterfaceDescriptor, bInterfaceSubClass)) {
                if (next == HID_SUBCLASS_BOOT) {
                    parser->state = ConfigParserStateFindingInterfaceProtocol;
                    Serial.print("Correct subclass\n");
                } else {
                    parser->state = ConfigParserStateFindingInterfaceClass;
                    Serial.print("Wrong subclass\n");
                }
            }
            break;
        case ConfigParserStateFindingInterfaceProtocol:
            if (parser->descType == DESC_TYPE_INTERFACE &&
                parser->descOffset == offsetof(struct InterfaceDescriptor, bInterfaceProtocol)) {
                if (next == HID_PROTOCOL_MOUSE) {
                    parser->state = ConfigParserStateFindingEndpoint;
                    Serial.print("Correct protocol\n");
                } else {
                    parser->state = ConfigParserStateFindingInterfaceClass;
                    Serial.print("Wrong protocol\n");
                }
            }
            break;
        case ConfigParserStateFindingEndpoint:
            if (parser->descType == DESC_TYPE_ENDPOINT &&
                parser->descOffset == offsetof(struct EndpointDescriptor, bEndpointAddress)) {
                parser->info->bootMouseEndpoint = next;
                parser->state = ConfigParserStateDone;
            }
    }

    parser->descOffset++;

    if (parser->descOffset == parser->descSize) {
        parser->descSize = 0;
        parser->descType = 0;
        parser->descOffset = 2;
    }
}

extern void debugPrintBuffer(uint8_t* data, uint8_t bytes);

void configParserPacketHandler(void* data, char* packetData, uint8_t packetSize, uint8_t offset) {
    struct ConfigParser* parser = (struct ConfigParser*)data;
    debugPrintBuffer(packetData, packetSize);
    for (size_t i = 0; i < packetSize; ++i) {
        configParserStep(parser, (uint8_t)packetData[i]);
    }
    debugPrintBuffer(packetData, packetSize);
}

void packetDebug(void* data, char* packetData, uint8_t packetSize, uint8_t offset) {
    debugPrintBuffer(packetData, packetSize);
}

void packetDirectCopy(void* data, char* packetData, uint8_t packetSize, uint8_t offset) {
    memcpy((char*)data + offset, packetData, packetSize);
}

bool getHIDInfo(struct HidInfo* result) {
    char readBuffer[18]; 
    
    if (!readControlTransfer(
        0, 
        REQUEST_TYPE_STANDARD | REQUEST_RECIPIENT_DEVICE,
        GET_DESCRIPTOR,
        PACK_WORD_BYTES(DESC_TYPE_DEVICE, 0x00),
        0x00,
        sizeof(struct DeviceDescriptor),
        readBuffer,
        packetDirectCopy
    )) {
        Serial.print("Failed to get device descriptor\n");
        return false;
    }

    uint8_t bDeviceClass = ((struct DeviceDescriptor*)readBuffer)->bDeviceClass;

    if (bDeviceClass != DEVICE_CLASS_DEVICE && bDeviceClass != DEVICE_CLASS_HID) {
        // unsupported device
        Serial.print("Unsupported device\n");
        return false;
    }

    uint8_t bNumConfigurations = 1;//((struct DeviceDescriptor*)readBuffer)->bNumConfigurations;

    for (uint8_t configurationIndex = 0; configurationIndex < bNumConfigurations; ++configurationIndex) {
        if (!readControlTransfer(
            0, 
            REQUEST_TYPE_STANDARD | REQUEST_RECIPIENT_DEVICE,
            GET_DESCRIPTOR,
            PACK_WORD_BYTES(DESC_TYPE_CONFIGURATION, configurationIndex),
            0x00,
            sizeof(struct ConfigurationDescriptor),
            readBuffer,
            packetDirectCopy
        )) {
            Serial.print("Failed to get config descriptor\n");
            return false;
        }

        uint16_t wTotalLength = ((struct ConfigurationDescriptor*)readBuffer)->wTotalLength;

        struct ConfigParser parser;
        configParserInit(&parser, result);

        if (!readControlTransfer(
            0, 
            REQUEST_TYPE_STANDARD | REQUEST_RECIPIENT_DEVICE,
            GET_DESCRIPTOR,
            PACK_WORD_BYTES(DESC_TYPE_CONFIGURATION, configurationIndex),
            0x00,
            wTotalLength,
            &parser,
            configParserPacketHandler
        )) {
            return false;
        }

        if (result->bootMouseEndpoint != 0) {
            result->bootMouseConfiguration = configurationIndex;
            return true;
        }
    }

    return false;
} 