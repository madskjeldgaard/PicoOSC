#include <cstdint>
#include <cstring>
#include <iostream>

#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include <climits>


namespace picoosc {

// From https://stackoverflow.com/questions/105252/how-do-i-convert-between-big-endian-and-little-endian-values-in-c
template <typename T>
T swap_endian(T u)
{
    static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");

    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}

using namespace std;

// This class represents a valid OSC message that can be sent over the via UDP
// as a UDP packet. It allows creating a buffer and adding data to it, and then
// sending it over the network.
//
// There are examples from the spec here:
// https://opensoundcontrol.stanford.edu/spec-1_0-examples.html#typetagstrings
class OSCMessage {
public:
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

  // Constructor
  OSCMessage() : mBuffer{0}, mBufferSize{0} {
	clear();
  }

  // Destructor
  ~OSCMessage() {
	// FIXME: CLEAR BUFFERS
  }

  // Add an OSC address to the message
  void addAddress(const char *address) {
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

  void padBuffer() {
    // Pad the buffer to the next 4-byte boundary
    while (mBufferSize % 4 != 0) {
      mBuffer[mBufferSize] = '\0';
      mBufferSize += 1;
    }
  }

  // Add 32 bit float with the 'f' osc type tag
  void addFloat(float value) {
    // Check if there is enough space in the buffer
    if (mBufferSize + 4 > MAX_MESSAGE_SIZE) {
	  printf("Not enough space in buffer\n");
      return;
    }

	// Add comma before type tag
    mBuffer[mBufferSize] = ',';
    mBufferSize += 1;

    // Add the type tag
    mBuffer[mBufferSize] = 'f';
    mBufferSize += 1;

	// Swap the bytes
	value = swap_endian<float>(value);

    padBuffer();

    // Add as 32 bit float
    std::memcpy(mBuffer + mBufferSize, &value, 4);
    mBufferSize += 4;

    // Pad the buffer to the next 4-byte boundary
    padBuffer();
  }

  // Add 32 bit integer with the 'i' osc type tag
  void addInt(int32_t value) {
    // Check if there is enough space in the buffer
    if (mBufferSize + 4 > MAX_MESSAGE_SIZE) {
      return;
    }

	// Add comma before type tag
    mBuffer[mBufferSize] = ',';
    mBufferSize += 1;

    // Add the type tag
    mBuffer[mBufferSize] = 'i';
    mBufferSize += 1;

	// The Pico is Little Endian, so we need to reverse the bytes
	value = swap_endian<int32_t>(value);

    padBuffer();

    // Add the integer
    std::memcpy(mBuffer + mBufferSize, &value, 4);
    mBufferSize += 4;

    // Pad the buffer to the next 4-byte boundary
    padBuffer();
  }

  // FIXME: There is something wrong with this function
  // It sometimes adds garbage to the end
  // Add string
  void addString(const char *value) {
    // Check if there is enough space in the buffer
    if (mBufferSize + std::strlen(value) + 1 > MAX_MESSAGE_SIZE) {
      return;
    }

	// Add a comma before the string
    mBuffer[mBufferSize] = ',';
    mBufferSize += 1;

    // Add the type tag
    mBuffer[mBufferSize] = 's';
    mBufferSize += 1;

    padBuffer();

	// Make tmp string to avoid modifying the original
	char tmp[std::strlen(value) + 1];

	// Swap endianness of all the bytes in the string
	for (std::size_t i = 0; i < std::strlen(value); i++) {

	  // Swap the bytes
	  tmp[i] = swap_endian<char>(value[i]);
	}


    // Add the string
    std::memcpy(mBuffer + mBufferSize, tmp, std::strlen(tmp));
    mBufferSize += std::strlen(tmp);

    padBuffer();

    // Add a null terminator
    mBuffer[mBufferSize] = '\0';
    mBufferSize += 1;

    // Pad the buffer to the next 4-byte boundary
    padBuffer();
  }

  // Add blob
  // void addBlob(const char *value, std::size_t size) {
  //   // Check if there is enough space in the buffer
  //   if (mBufferSize + size + 4 > MAX_MESSAGE_SIZE) {
  //     return;
  //   }
  //
  //   // Add the type tag
  //   mBuffer[mBufferSize] = 'b';
  //   mBufferSize += 1;
  //
  //   // Add the blob size
  //   std::memcpy(mBuffer + mBufferSize, &size, 4);
  //   mBufferSize += 4;
  //
  //   // Add the blob
  //   std::memcpy(mBuffer + mBufferSize, value, size);
  //   mBufferSize += size;
  //
  //   // Pad the buffer to the next 4-byte boundary
  //   padBuffer();
  // }

  // Get the data
  const char *data() const { return mBuffer; }

  // Get the size
  std::size_t size() const { return mBufferSize; }

  // Clear the buffer
  void clear() {
    mBufferSize = 0;
    std::memset(mBuffer, 0, MAX_MESSAGE_SIZE);
  }

  // Convert the internal buffer to a pbuf to be sent via udp with lwip
  void asPbuf(struct pbuf *p) {
    // Copy the buffer to the pbuf
    std::memcpy(p->payload, mBuffer, mBufferSize);
  }

  // Send the message over the network
  void send(struct udp_pcb *pcb, const ip_addr_t *ip, uint16_t port) {
	printf("Trying to send osc packet\n");

    // Create a pbuf
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, mBufferSize, PBUF_RAM);

    // Copy the buffer to the pbuf
    std::memcpy(p->payload, mBuffer, mBufferSize);

    // Send the pbuf
    const auto error = udp_sendto(pcb, p, ip, port);

    if (error != ERR_OK) {
      printf("Failed to send UDP packet! error=%d\n", error);
    } else {
      printf("Sent packet \n");
    };

	print();

    // Free the pbuf
    pbuf_free(p);
  }

  // Print the buffer
  void print() {
    for (std::size_t i = 0; i < mBufferSize; i++) {
      std::cout << mBuffer[i];
    }
    std::cout << std::endl;
  }

private:
  // Buffer
  std::size_t mBufferSize;

  // A buffer based on char as the basic type
  char mBuffer[MAX_MESSAGE_SIZE];
};

} // namespace picoosc
