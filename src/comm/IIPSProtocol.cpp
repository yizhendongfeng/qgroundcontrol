#ifdef __QNXNTO__
    #include <string.h>
    #define nullptr 0
#else
    #include <cstring>
#endif

#include "IIPSProtocol.h"

BinaryBuffer IIPSProtocol::BinaryPacketEncode(BinaryPacket binaryPacket)
{
    BinaryBuffer binaryBuffer;

    binaryBuffer.data[1] = static_cast<unsigned char>(binaryPacket.id);
    binaryBuffer.data[2] = static_cast<unsigned char>(binaryPacket.dataLength);
    unsigned short crc16Ccitt = CalculateCrc16Ccitt(binaryPacket.data, binaryPacket.dataLength);
    memcpy(binaryBuffer.data + 3, &crc16Ccitt, sizeof(unsigned short));
    binaryBuffer.data[0] = CalculateHeaderLrc(binaryBuffer.data + 1);
    memcpy(binaryBuffer.data + 5, binaryPacket.data, binaryPacket.dataLength);

    binaryBuffer.length = BinaryPacketHeaderLength + binaryPacket.dataLength;
    binaryBuffer.errors = 0;

    return binaryBuffer;
}

BinaryPacket IIPSProtocol::BinaryBufferDecode(BinaryBuffer* binaryBuffer)
{
    BinaryPacket binaryPacket = { nullptr, IdError, DataLengthError };

    while (binaryBuffer->length >= BinaryPacketHeaderLength)
    {
        // Find the start of a packet by scanning for a valid lrc
        if (binaryBuffer->data[0] != CalculateHeaderLrc(binaryBuffer->data + 1))
        {
            memcpy(binaryBuffer->data, binaryBuffer->data + 1, --binaryBuffer->length);
            continue;
        }
        // Continue to receive and write data to buffer
        const int packetDataLength = binaryBuffer->data[2];
        const int packetLength = BinaryPacketHeaderLength + packetDataLength;
        if (binaryBuffer->length < packetLength)
            break;
        // Check crc16-ccitt of the buffer
        if ((binaryBuffer->data[3] | binaryBuffer->data[4] << 8) != CalculateCrc16Ccitt(binaryBuffer->data + BinaryPacketHeaderLength, packetDataLength))
        {
            memcpy(binaryBuffer->data, binaryBuffer->data + 1, --binaryBuffer->length);
            binaryBuffer->errors++;
            continue;
        }
        // Decode one packet from the buffer
        binaryPacket.id = binaryBuffer->data[1];
        binaryPacket.dataLength = packetDataLength;
        binaryPacket.data = new unsigned char[packetDataLength];
        memcpy(binaryPacket.data, binaryBuffer->data + BinaryPacketHeaderLength, packetDataLength);
        // Delete decoded packet from the buffer
        binaryBuffer->length -= packetLength;
        memcpy(binaryBuffer->data, binaryBuffer->data + packetLength, binaryBuffer->length);
        break;
    }

    return binaryPacket;
}

AsciiBuffer IIPSProtocol::AsciiPacketEncode(AsciiPacket asciiPacket)
{
    AsciiBuffer asciiBuffer;

    asciiBuffer.data[0] = '$';
    memcpy(asciiBuffer.data + 1, asciiPacket.data, asciiPacket.dataLength);
    asciiBuffer.data[asciiPacket.dataLength + 1] = '*';
    unsigned short checksum = CalculateChecksum(asciiPacket.data, asciiPacket.dataLength);
    memcpy(asciiBuffer.data + asciiPacket.dataLength + 2, &checksum, sizeof(unsigned short));
    asciiBuffer.data[asciiPacket.dataLength + 4] = '\r';
    asciiBuffer.data[asciiPacket.dataLength + 5] = '\n';
    asciiBuffer.data[asciiPacket.dataLength + 6] = '\0';

    asciiBuffer.length = asciiPacket.dataLength + AsciiPacketHeaderLength;
    asciiBuffer.errors = 0;

    return asciiBuffer;
}

AsciiPacket IIPSProtocol::AsciiBufferDecode(AsciiBuffer* asciiBuffer)
{
    AsciiPacket asciiPacket = { nullptr, DataLengthError };

    while (asciiBuffer->length >= AsciiPacketHeaderLength)
    {
        // Find the start of a packet by scanning a valid $
        if (asciiBuffer->data[0] != '$')
        {
            memcpy(asciiBuffer->data, asciiBuffer->data + 1, --asciiBuffer->length);
            continue;
        }
        // Continue to receive and write data to buffer
        int packetDataLength = DataLengthError;
        for (int i = 1; i < asciiBuffer->length; i++)
            if (asciiBuffer->data[i] == '*')
                packetDataLength = i - 1;
        if (packetDataLength == DataLengthError)
            break;
        const int packetLength = AsciiPacketHeaderLength + packetDataLength;
        if (asciiBuffer->length < packetLength)
            break;
        // Check checksum of the buffer
        if ((asciiBuffer->data[packetDataLength + 2] | asciiBuffer->data[packetDataLength + 3] << 8) != CalculateChecksum(asciiBuffer->data + 1, packetDataLength))
        {
            memcpy(asciiBuffer->data, asciiBuffer->data + 1, --asciiBuffer->length);
            asciiBuffer->errors++;
            continue;
        }
        // Decode one packet from the buffer
        asciiPacket.dataLength = packetDataLength;
        const int storeLength = packetDataLength + 1;
        asciiPacket.data = new unsigned char[storeLength]; // Add 1 byte to store \0
        memcpy(asciiPacket.data, asciiBuffer->data + 1, packetDataLength);
        asciiPacket.data[packetDataLength] = '\0';
        // Delete decoded packet from the buffer
        asciiBuffer->length -= packetLength;
        memcpy(asciiBuffer->data, asciiBuffer->data + packetLength, asciiBuffer->length);
        break;
    }

    return asciiPacket;
}

SbusBuffer IIPSProtocol::SbusPacketEncode(SbusPacket sbusPacket)
{
    SbusBuffer sbusBuffer;

    sbusBuffer.data[0] = 0x0f;
    sbusBuffer.data[1] = static_cast<unsigned char>(sbusPacket.channels[0]);
    sbusBuffer.data[2] = static_cast<unsigned char>(sbusPacket.channels[0] >> 8 | sbusPacket.channels[1] << 3);
    sbusBuffer.data[3] = static_cast<unsigned char>(sbusPacket.channels[1] >> 5 | sbusPacket.channels[2] << 6);
    sbusBuffer.data[4] = static_cast<unsigned char>(sbusPacket.channels[2] >> 2);
    sbusBuffer.data[5] = static_cast<unsigned char>(sbusPacket.channels[2] >> 10 | sbusPacket.channels[3] << 1);
    sbusBuffer.data[6] = static_cast<unsigned char>(sbusPacket.channels[3] >> 7 | sbusPacket.channels[4] << 4);
    sbusBuffer.data[7] = static_cast<unsigned char>(sbusPacket.channels[4] >> 4 | sbusPacket.channels[5] << 7);
    sbusBuffer.data[8] = static_cast<unsigned char>(sbusPacket.channels[5] >> 1);
    sbusBuffer.data[9] = static_cast<unsigned char>(sbusPacket.channels[5] >> 9 | sbusPacket.channels[6] << 2);
    sbusBuffer.data[10] = static_cast<unsigned char>(sbusPacket.channels[6] >> 6 | sbusPacket.channels[7] << 5);
    sbusBuffer.data[11] = static_cast<unsigned char>(sbusPacket.channels[7] >> 3);
    sbusBuffer.data[12] = static_cast<unsigned char>(sbusPacket.channels[8]);
    sbusBuffer.data[13] = static_cast<unsigned char>(sbusPacket.channels[8] >> 8 | sbusPacket.channels[9] << 3);
    sbusBuffer.data[14] = static_cast<unsigned char>(sbusPacket.channels[9] >> 5 | sbusPacket.channels[10] << 6);
    sbusBuffer.data[15] = static_cast<unsigned char>(sbusPacket.channels[10] >> 2);
    sbusBuffer.data[16] = static_cast<unsigned char>(sbusPacket.channels[10] >> 10 | sbusPacket.channels[11] << 1);
    sbusBuffer.data[17] = static_cast<unsigned char>(sbusPacket.channels[11] >> 7 | sbusPacket.channels[12] << 4);
    sbusBuffer.data[18] = static_cast<unsigned char>(sbusPacket.channels[12] >> 4 | sbusPacket.channels[13] << 7);
    sbusBuffer.data[19] = static_cast<unsigned char>(sbusPacket.channels[13] >> 1);
    sbusBuffer.data[20] = static_cast<unsigned char>(sbusPacket.channels[13] >> 9 | sbusPacket.channels[14] << 2);
    sbusBuffer.data[21] = static_cast<unsigned char>(sbusPacket.channels[14] >> 6 | sbusPacket.channels[15] << 5);
    sbusBuffer.data[22] = static_cast<unsigned char>(sbusPacket.channels[15] >> 3);
    sbusBuffer.data[23] = sbusPacket.flags;
    sbusBuffer.data[24] = 0x04;

    sbusBuffer.length = SbusPacketLength;
    sbusBuffer.errors = 0;

    return sbusBuffer;
}

SbusPacket IIPSProtocol::SbusBufferDecode(SbusBuffer* sbusBuffer)
{
    SbusPacket sbusPacket = { { ChannelError }, 0 };

    while (sbusBuffer->length >= SbusPacketLength)
    {
        // Find the start of a packet by scanning a valid 0x0f
        if (sbusBuffer->data[0] != 0x0f)
        {
            memcpy(sbusBuffer->data, sbusBuffer->data + 1, --sbusBuffer->length);
            continue;
        }
        // Check end byte of the buffer
        if (sbusBuffer->data[24] != 0x04 && sbusBuffer->data[24] != 0x14 && sbusBuffer->data[24] != 0x24 && sbusBuffer->data[24] != 0x34)
        {
            memcpy(sbusBuffer->data, sbusBuffer->data + 1, --sbusBuffer->length);
            sbusBuffer->errors++;
            continue;
        }
        // Decode one packet from the buffer
        sbusPacket.channels[0] = (sbusBuffer->data[1] | sbusBuffer->data[2] << 8) & 0x07ff;
        sbusPacket.channels[1] = (sbusBuffer->data[2] >> 3 | sbusBuffer->data[3] << 5) & 0x07ff;
        sbusPacket.channels[2] = (sbusBuffer->data[3] >> 6 | sbusBuffer->data[4] << 2 | sbusBuffer->data[5] << 10) & 0x07ff;
        sbusPacket.channels[3] = (sbusBuffer->data[5] >> 1 | sbusBuffer->data[6] << 7) & 0x07ff;
        sbusPacket.channels[4] = (sbusBuffer->data[6] >> 4 | sbusBuffer->data[7] << 4) & 0x07ff;
        sbusPacket.channels[5] = (sbusBuffer->data[7] >> 7 | sbusBuffer->data[8] << 1 | sbusBuffer->data[9] << 9) & 0x07ff;
        sbusPacket.channels[6] = (sbusBuffer->data[9] >> 2 | sbusBuffer->data[10] << 6) & 0x07ff;
        sbusPacket.channels[7] = (sbusBuffer->data[10] >> 5 | sbusBuffer->data[11] << 3) & 0x07ff;
        sbusPacket.channels[8] = (sbusBuffer->data[12] | sbusBuffer->data[13] << 8) & 0x07ff;
        sbusPacket.channels[9] = (sbusBuffer->data[13] >> 3 | sbusBuffer->data[14] << 5) & 0x07ff;
        sbusPacket.channels[10] = (sbusBuffer->data[14] >> 6 | sbusBuffer->data[15] << 2 | sbusBuffer->data[16] << 10) & 0x07ff;
        sbusPacket.channels[11] = (sbusBuffer->data[16] >> 1 | sbusBuffer->data[17] << 7) & 0x07ff;
        sbusPacket.channels[12] = (sbusBuffer->data[17] >> 4 | sbusBuffer->data[18] << 4) & 0x07ff;
        sbusPacket.channels[13] = (sbusBuffer->data[18] >> 7 | sbusBuffer->data[19] << 1 | sbusBuffer->data[20] << 9) & 0x07ff;
        sbusPacket.channels[14] = (sbusBuffer->data[20] >> 2 | sbusBuffer->data[21] << 6) & 0x07ff;
        sbusPacket.channels[15] = (sbusBuffer->data[21] >> 5 | sbusBuffer->data[22] << 3) & 0x07ff;
        sbusPacket.flags = sbusBuffer->data[23];
        // Delete decoded packet from the buffer
        sbusBuffer->length -= SbusPacketLength;
        memcpy(sbusBuffer->data, sbusBuffer->data + SbusPacketLength, sbusBuffer->length);
        break;
    }

    return sbusPacket;
}

unsigned short IIPSProtocol::CalculateCrc16Ccitt(const unsigned char* data, int length)
{
    unsigned short crc16Ccitt = 0xffff;
    static unsigned short crc16CcittArray[256] =
    {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
    };
    for (int i = 0; i < length; i++)
        crc16Ccitt = static_cast<unsigned short>(crc16Ccitt << 8 ^ crc16CcittArray[crc16Ccitt >> 8 ^ data[i]]);
    return crc16Ccitt;
}

unsigned char IIPSProtocol::CalculateHeaderLrc(const unsigned char* data)
{
    return ((data[0] + data[1] + data[2] + data[3]) ^ 0xff) + 1;
}

unsigned short IIPSProtocol::CalculateChecksum(const unsigned char* data, int length)
{
    unsigned char u8Checksum = 0;
    for (int i = 0; i < length; i++)
        u8Checksum ^= data[i];

    const unsigned char left = (u8Checksum & 0xf0) >> 4;
    const unsigned char right = u8Checksum & 0x0f;

    unsigned short checksum = right > 0x09 ? right + 'A' - 0x0a : right + '0';
    checksum = static_cast<unsigned short>((checksum << 8) + (left > 0x09 ? left + 'A' - 0x0a : left + '0'));

    return checksum;
}
