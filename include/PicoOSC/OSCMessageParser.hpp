#pragma once

#include <cstdio>
#include <cstring>
#include <vector>

#include "OSC_utils.hpp"

namespace picoosc
{

class OSCMessageParser
{
public:
  static void parse(const char* data, size_t size)
  {
    // Verify that the data is valid OSC data by checking if the first byte is a
    // '/'
    if (&data[0] != "/") {
      std::vector<char> headerData;
      std::vector<char> typeTagData;
      std::vector<char> argumentsData;

      // Parse header
      for (size_t i = 0; i < size; i++) {
        if (data[i] == ',') {
          // Address is done
          const auto addressSize = i;

          // Copy address
          headerData.resize(addressSize);
          std::memcpy(headerData.data(), data, addressSize);

        } else if (data[i] == '\0') {
          // Type tag is done
          // Copy type tag
          const auto typeTagSize = i - headerData.size() - 1;
          typeTagData.resize(typeTagSize);
          std::memcpy(
              typeTagData.data(), data + headerData.size() + 1, typeTagSize);
        } else {
          // Copy arguments
          const auto argumentsSize =
              size - headerData.size() - typeTagData.size() - 2;

          argumentsData.resize(argumentsSize);

          if (argumentsSize > 0) {
            // Pad buffer
            padBuffer(argumentsData.data(), argumentsSize);

            // TODO: Constrain source and destination to 4 bytes

            std::memcpy(argumentsData.data(),
                        data + headerData.size() + typeTagData.size() + 2,
                        argumentsSize);
          }
        }
      };

      // Print contents
      printf("Address: %s\n", headerData.data());
      printf("Type tag: %s\n", typeTagData.data());

      if (typeTagData.size() > 0) {
        if (typeTagData[0] == 'f') {
          float value;

          std::memcpy(&value, argumentsData.data(), sizeof(float));

          value = swap_endian<float>(value);

          printf("Value: %f\n", value);
        } else if (typeTagData[0] == 'i') {
          int32_t value;

          // Swap endianness
          std::memcpy(&value, argumentsData.data(), sizeof(int32_t));

          value = swap_endian<int32_t>(value);

          printf("Value: %d\n", value);
        } else if (typeTagData[0] == 's') {
          printf("Value: %s\n", argumentsData.data());
        }
      }
    }
  }

  static void padBuffer(char* buffer, size_t size)
  {
    // Pad the buffer with null terminators until it's a size of 4 bytes
    while (size % 4 != 0) {
      buffer[size] = '\0';
      size += 1;
    }
  }
};

};  // namespace picoosc
