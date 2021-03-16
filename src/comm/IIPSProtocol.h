/// ---------------------------------------------------------------------------
/// Copyright (C) 2020 by 熊俊峰. All rights reserved.
/// @file IIPSProtocol.h
/// @date 2020/3/15
///
/// @author 熊俊峰
/// Contact: xjf_whut@qq.com
///
/// @brief Standard packet IIPSProtocol
///
/// Binary packet structure: HeaderLrc(u8) PacketId(u8) PacketDataLength(u8) CRC16(u16) PacketData
/// Ascii packet structure: $PacketData(Id,Data1,Data2,...,Datan)*Checksum\r\n
/// Sbus packet structure: StartByte(0x0f) PacketData(Data0 Data1 … Data21 Flags) EndByte(0x00)
/// Error: id = -1 or dataLength = -1 or channel = 65535
///
/// @version 3.0
/// @note Please feel free to contact me if you have any questions
/// ---------------------------------------------------------------------------

#pragma once

#define IdError (-1) // NOLINT(cppcoreguidelines-macro-usage)
#define DataLengthError (-1) // NOLINT(cppcoreguidelines-macro-usage)
#define ChannelError 65535 // NOLINT(cppcoreguidelines-macro-usage)

const int BinaryPacketHeaderLength = 5;
const int MaxPacketDataLength = 255;
const int BinaryBufferLength = (BinaryPacketHeaderLength + MaxPacketDataLength) * 2;

const int AsciiPacketHeaderLength = 6; // The dataLength except packet data
const int AsciiBufferLength = (AsciiPacketHeaderLength + MaxPacketDataLength) * 2 + 1; // Add 1 byte to store \0

const int SbusChannelsLength = 16;
const int SbusPacketLength = 25;
const int SbusBufferLength = SbusPacketLength * 2;

typedef struct
{
    unsigned char* data;
    int id;
    int dataLength;
} BinaryPacket;

typedef struct
{
    unsigned char data[BinaryBufferLength];
    int length;
    unsigned errors;
} BinaryBuffer;

typedef struct
{
    unsigned char* data;
    int dataLength;
} AsciiPacket;

typedef struct
{
    unsigned char data[AsciiBufferLength];
    int length;
    unsigned errors;
} AsciiBuffer;

typedef struct
{
    unsigned short channels[SbusChannelsLength];
    unsigned char flags;
} SbusPacket;

typedef struct
{
    unsigned char data[SbusBufferLength];
    int length;
    unsigned errors;
} SbusBuffer;

class IIPSProtocol
{
public:
    static BinaryBuffer BinaryPacketEncode(BinaryPacket binaryPacket);
    static BinaryPacket BinaryBufferDecode(BinaryBuffer* binaryBuffer);
    static AsciiBuffer AsciiPacketEncode(AsciiPacket asciiPacket);
    static AsciiPacket AsciiBufferDecode(AsciiBuffer* asciiBuffer);
    static SbusBuffer SbusPacketEncode(SbusPacket sbusPacket);
    static SbusPacket SbusBufferDecode(SbusBuffer* sbusBuffer);

private:
    static unsigned short CalculateCrc16Ccitt(const unsigned char* data, int length);
    static unsigned char CalculateHeaderLrc(const unsigned char* data);
    static unsigned short CalculateChecksum(const unsigned char* data, int length);
};
