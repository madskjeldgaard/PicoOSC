#pragma once
#include <climits>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "OSCClient.hpp"
#include "OSC_utils.hpp"

namespace picoosc
{

// The maximum size of an OSC message is 1024 bytes.
static constexpr auto MAX_MESSAGE_SIZE = 1024;

// The maximum size of an OSC address is 255 bytes.
static constexpr auto MAX_ADDRESS_SIZE = 255;

// The maximum size of an OSC type tag string is 255 bytes.
static constexpr auto MAX_TYPE_TAG_SIZE = 255;

// The maximum number of arguments in an OSC message is 64.
static constexpr auto MAX_ARGUMENTS = 64;

// The maximum size of an OSC argument is 1024 bytes.
static constexpr auto MAX_ARGUMENT_SIZE = 1024;

// The maximum size of an OSC bundle is 1024 bytes.
static constexpr auto MAX_BUNDLE_SIZE = 1024;

// The maximum number of messages in an OSC bundle is 255.
static constexpr auto MAX_MESSAGES = 255;

// The maximum size of an OSC bundle's timestamp is 8 bytes.
static constexpr auto MAX_TIMESTAMP_SIZE = 8;

// The maximum size of an OSC bundle's timestamp is 8 bytes.
static constexpr auto MAX_TIMETAG_SIZE = 8;

// This class represents a valid OSC message that can be sent over the via UDP
// as a UDP packet. It allows creating a buffer and adding data to it, and then
// sending it over the network.
//
// There are examples from the spec here:
// https://opensoundcontrol.stanford.edu/spec-1_0-examples.html#typetagstrings
class OSCMessage
{
public:
  // Constructor
  OSCMessage()
      : mBuffer {0}
      , mBufferSize {0}
  {
    clear();
  }

  // Destructor
  // ~OSCMessage()
  // {
  //  // Clean up
  //
  // }

  // Add an OSC address to the message
  void addAddress(const char* address)
  {
    // Check if the address is too long
    if (strlen(address) > MAX_ADDRESS_SIZE) {
      return;
    }

    // Add the address to the buffer
    std::memcpy(mBuffer + mBufferSize, address, std::strlen(address));
    mBufferSize += std::strlen(address);

    // Add a null terminator
    mBuffer[mBufferSize] = '\0';
    mBufferSize += 1;

    // Pad the buffer to the next 4-byte boundary
    padBuffer();
  }

  void padBuffer()
  {
    // Pad the buffer to the next 4-byte boundary
    while (mBufferSize % 4 != 0) {
      mBuffer[mBufferSize] = '\0';
      mBufferSize += 1;
    }
  }

  // Use templates and constexpr if to add the correct type tag to the buffer
  // and then add the value to the buffer
  template<typename T>
  void add(T value)
  {
    // Check if there is enough space in the buffer
    if (mBufferSize + 4 > MAX_MESSAGE_SIZE) {
      printf("Not enough space in buffer\n");
      return;
    }

    constexpr auto isFloat = std::is_same<T, float>::value;
    constexpr auto isInt = std::is_same<T, int32_t>::value;
    constexpr auto isString = std::is_same<T, const char*>::value;

    // Add type tag
    // Add comma before type tag
    mBuffer[mBufferSize] = ',';
    mBufferSize += 1;

    if constexpr (isFloat) {
      mBuffer[mBufferSize] = 'f';
    } else if constexpr (isInt) {
      mBuffer[mBufferSize] = 'i';
    } else if constexpr (isString) {
      mBuffer[mBufferSize] = 's';
    } else {
      printf("Unsupported type\n");
    }

    mBufferSize += 1;

    // For simple types like floats and integers we can just copy the value to
    // the buffer
    if constexpr (isFloat || isInt) {
      // The Raspberry Pi Pico is Little Endian so we need to swap the bytes in
      // the vlaue
      value = swap_endian<T>(value);
      padBuffer();

      // Copy to the buffer
      std::memcpy(mBuffer + mBufferSize, &value, 4);
      mBufferSize += 4;

    } else if constexpr (isString) {
      // For strings it's a bit more complex

      // Add a null terminator
      mBuffer[mBufferSize] = '\0';
      mBufferSize += 1;

      // Swap the endianness of all chars
      for (int i = 0; i < std::strlen(value); i++) {
        mBuffer[mBufferSize + i] = swap_endian<char>(value[i]);
      }

      // Add a null terminator
      mBuffer[mBufferSize] = '\0';
      mBufferSize += 1;

      padBuffer();

      // Copy the string to the buffer
      std::memcpy(mBuffer + mBufferSize, value, std::strlen(value));
      mBufferSize += std::strlen(value);

    } else {
      printf("Unsupported type\n");
      return;
    }

    padBuffer();
  }

  // Get the data
  const char* data() const
  {
    return mBuffer;
  }

  // Get the size
  std::size_t size() const
  {
    return mBufferSize;
  }

  // Clear the buffer
  void clear()
  {
    mBufferSize = 0;
    std::memset(mBuffer, 0, MAX_MESSAGE_SIZE);
  }

  // Send the message over the network using a OSC Client
  auto send(OSCClient& client) -> bool
  {
    const auto error = client.send(mBuffer, mBufferSize);

    return error == ERR_OK;
  }

  // Print the buffer
  void print()
  {
    for (std::size_t i = 0; i < mBufferSize; i++) {
      std::cout << mBuffer[i];
    }
    std::cout << std::endl;
  }

  // Decode a udp packet buffer

private:
  // Buffer
  std::size_t mBufferSize;

  // A buffer based on char as the basic type
  char mBuffer[MAX_MESSAGE_SIZE];
};

}  // namespace picoosc
