#include <climits>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "lwip/pbuf.h"
#include "lwip/udp.h"

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

// The maximum size of an OSC bundle's timetag is 8 bytes.
static constexpr auto MAX_TIMETAG_SIZE = 8;

// From
// https://stackoverflow.com/questions/105252/how-do-i-convert-between-big-endian-and-little-endian-values-in-c
template<typename T>
auto swap_endian(T uns) -> T
{
  static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");

  union
  {
    T uns;
    unsigned char u8[sizeof(T)];
  } source, dest;

  source.uns = uns;

  for (size_t k = 0; k < sizeof(T); k++) {
      dest.u8[k] = source.u8[sizeof(T) - k - 1];
  }

  return dest.u;
}

using namespace std;

// A class representing a UDP client, mainly used for sending packets
class OSCClient
{
public:
  OSCClient(const OSCClient&) = default;
  OSCClient(OSCClient&&) = delete;
  auto operator=(const OSCClient&) -> OSCClient& = default;
  auto operator=(OSCClient&&) -> OSCClient& = delete;
  // Constructor that takes an address as a string and a port number
  OSCClient(const char* address, uint16_t port)
  {
    // Convert the address string to an ip_addr_t
    ipaddr_aton(address, &m_addr);

    // Set the port
    this->m_port = port;

    // Create a new OSC pcb
    m_pcb = udp_new();

    // Bind the pcb to the port
    // const auto result = udp_bind(mPcb, &mAddr, port);
    //
    // if (result != ERR_OK) {
    //   printf("Failed to bind UDP pcb! error=%d\n", result);
    // } else {
    //   printf("Bound UDP pcb to port %d\n", port);
    // }
  }

  // Destructor
  ~OSCClient()
  {
    // Close the pcb
    udp_remove(m_pcb);
  }

  // Send packet
  auto send(const char* buffer, uint16_t size)
  {
    // Create a pbuf
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);

    // Copy the buffer to the pbuf
    std::memcpy(p->payload, buffer, size);

    // Attempt to send the packet
    const auto error = udp_sendto(m_pcb, p, &m_addr, m_port);

    // Free packet buffer
    pbuf_free(p);

    if (error != ERR_OK) {
      printf("Failed to send UDP packet! error=%d\n", error);
      return 1;
    } else {
      printf("Sent packet \n");
      return 0;
    };
  }

private:
  udp_pcb* m_pcb{};
  ip_addr_t m_addr{};
  uint16_t m_port;
};

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
      : m_buffer {0}
      , m_bufferSize {0}
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
    std::memcpy(m_buffer + m_bufferSize, address, std::strlen(address));
    m_bufferSize += std::strlen(address);

    // Add a null terminator
    m_buffer[m_bufferSize] = '\0';
    m_bufferSize += 1;

    // Pad the buffer to the next 4-byte boundary
    padBuffer();
  }

  void padBuffer()
  {
    // Pad the buffer to the next 4-byte boundary
    while (m_bufferSize % 4 != 0) {
      m_buffer[m_bufferSize] = '\0';
      m_bufferSize += 1;
    }
  }

  // Use templates and constexpr if to add the correct type tag to the buffer
  // and then add the value to the buffer
  template<typename T>
  void add(T value)
  {
    // Check if there is enough space in the buffer
    if (m_bufferSize + 4 > MAX_MESSAGE_SIZE) {
      printf("Not enough space in buffer\n");
      return;
    }

    constexpr auto isFloat = std::is_same<T, float>::value;
    constexpr auto isInt = std::is_same<T, int32_t>::value;
    constexpr auto isString = std::is_same<T, const char*>::value;

    // Add type tag
    // Add comma before type tag
    m_buffer[m_bufferSize] = ',';
    m_bufferSize += 1;

    if constexpr (isFloat) {
      m_buffer[m_bufferSize] = 'f';
    } else if constexpr (isInt) {
      m_buffer[m_bufferSize] = 'i';
    } else if constexpr (isString) {
      m_buffer[m_bufferSize] = 's';
    } else {
      printf("Unsupported type\n");
    }

    m_bufferSize += 1;

    // For simple types like floats and integers we can just copy the value to
    // the buffer
    if constexpr (isFloat || isInt) {
      // The Raspberry Pi Pico is Little Endian so we need to swap the bytes in
      // the vlaue
      value = swap_endian<T>(value);
      padBuffer();

      // Copy to the buffer
      std::memcpy(m_buffer + m_bufferSize, &value, 4);
      m_bufferSize += 4;

    } else if constexpr (isString) {
      // For strings it's a bit more complex

      // Add a null terminator
      m_buffer[m_bufferSize] = '\0';
      m_bufferSize += 1;

      // Swap the endianness of all chars
      for (int i = 0; i < std::strlen(value); i++) {
        m_buffer[m_bufferSize + i] = swap_endian<char>(value[i]);
      }

      // Add a null terminator
      m_buffer[m_bufferSize] = '\0';
      m_bufferSize += 1;

      padBuffer();

      // Copy the string to the buffer
      std::memcpy(m_buffer + m_bufferSize, value, std::strlen(value));
      m_bufferSize += std::strlen(value);

    } else {
      printf("Unsupported type\n");
      return;
    }

    padBuffer();
  }

  // Get the data
  auto data() const -> const char*
  {
    return m_buffer;
  }

  // Get the size
  auto size() const -> std::size_t
  {
    return m_bufferSize;
  }

  // Clear the buffer
  void clear()
  {
    m_bufferSize = 0;
    std::memset(m_buffer, 0, MAX_MESSAGE_SIZE);
  }

  // Send the message over the network using a OSC Client
  auto send(OSCClient& client) -> bool
  {
    const auto error = client.send(m_buffer, m_bufferSize);

    return error == ERR_OK;
  }

  // Print the buffer
  void print()
  {
    for (std::size_t i = 0; i < m_bufferSize; i++) {
      std::cout << m_buffer[i];
    }
    std::cout << std::endl;
  }

  // Decode a udp packet buffer

private:
  // Buffer
  std::size_t m_bufferSize;

  // A buffer based on char as the basic type
  char m_buffer[MAX_MESSAGE_SIZE];
};

}  // namespace picoosc
