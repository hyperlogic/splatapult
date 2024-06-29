/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "binaryattribute.h"

static uint32_t propertyTypeSizeArr[(size_t)BinaryAttribute::Type::NumTypes] = {
    0, // Unknown
    sizeof(int8_t), // Char
    sizeof(uint8_t), // UChar
    sizeof(int16_t), // Short
    sizeof(uint16_t), // UShort
    sizeof(int32_t), // Int
    sizeof(uint32_t), // UInt
    sizeof(float), // Float
    sizeof(double) // Double
};

BinaryAttribute::BinaryAttribute(Type typeIn, size_t offsetIn) :
    type(typeIn),
    size(propertyTypeSizeArr[(uint32_t)typeIn]),
    offset(offsetIn)
{
    ;
}
