#include <climits>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <variant>

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

// The maximum size of an OSC bundle's timestamp is 8 bytes.
static constexpr auto MAX_TIMETAG_SIZE = 8;

// From
// https://stackoverflow.com/questions/105252/how-do-i-convert-between-big-endian-and-little-endian-values-in-c
template<typename T>
T swap_endian(T u)
{
  static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");

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

// A class representing a UDP client, mainly used for sending packets
class OSCClient
{
public:
  // Constructor that takes an address as a string and a port number
  OSCClient(const char* address, uint16_t port)
  {
    // Convert the address string to an ip_addr_t
    ipaddr_aton(address, &mAddr);

    // Set the port
    this->mPort = port;

    // Create a new OSC pcb
    mPcb = udp_new();

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
    udp_remove(mPcb);
  }

  // Send packet
  auto send(const char* buffer, uint16_t size)
  {
    // Create a pbuf
    struct pbuf* p = pbuf_alloc(PBUF_TRANSPORT, size, PBUF_RAM);

    // Copy the buffer to the pbuf
    std::memcpy(p->payload, buffer, size);

    // Attempt to send the packet
    const auto error = udp_sendto(mPcb, p, &mAddr, mPort);

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
  udp_pcb* mPcb;
  ip_addr_t mAddr;
  uint16_t mPort;
};

// A simple OSC server that can receive OSC messages using lwip
class OSCServer
{
public:
  // Constructor that takes a port number
  OSCServer(uint16_t port)
  {
    // Create a new OSC pcb
    mPcb = udp_new();

    // Bind the pcb to the port
    const auto result = udp_bind(mPcb, IP_ADDR_ANY, port);

    if (result != ERR_OK) {
      printf("Failed to bind UDP pcb! error=%d\n", result);
    } else {
      printf("Bound UDP pcb to port %d\n", port);
    }
  }

  // Destructor
  ~OSCServer()
  {
    // Close the pcb
    udp_remove(mPcb);
  }

  // Receive packet
  auto receive(char* buffer, uint16_t size)
  {
    // Attempt to receive the packet
    udp_recv(
        mPcb,
        [](void* arg, udp_pcb* pcb, pbuf* p, const ip_addr_t* addr, u16_t port)
        {
          // Copy the buffer to the pbuf
          std::memcpy(arg, p->payload, p->len);

          // Free packet buffer
          pbuf_free(p);
        },
        buffer);
  }

private:
  udp_pcb* mPcb;
};

// This class represents a valid OSC message that can be sent over the via
// UDP as a UDP packet. It allows creating a buffer and adding data to it,
// and then sending it over the network.
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

  // Use templates and constexpr if to add the correct type tag to the
  // buffer and then add the value to the buffer
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

    // For simple types like floats and integers we can just copy the value
    // to the buffer
    if constexpr (isFloat || isInt) {
      // The Raspberry Pi Pico is Little Endian so we need to swap the bytes
      // in the vlaue
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

  // Decode a udp packet buffer as an OSC message
  void parse(const char* buffer, std::size_t size)
  {
    // Clear the buffer
    clear();

    // Copy the buffer
    std::memcpy(mBuffer, buffer, size);
    mBufferSize = size;

    // Iterate over all bytes in buffer and decode the address, type tag and
    // arguments from the bytes

    // State machine
    enum State
    {
      ADDRESS,
      TYPE_TAG,
      ARGUMENTS
    };

    State state = ADDRESS;

    // Index of current byte
    int i = 0;

    // Index of start of address
    int addressStart = 0;

    // Index of end of address
    int addressEnd = 0;

    // Index of start of type tag
    int typeTagStart = 0;

    // Index of end of type tag
    int typeTagEnd = 0;

    // Iterate over all bytes in buffer
    while (i < size) {
      // Get the current byte
      uint8_t byte = buffer[i];

      // Decode the byte based on the current state
      switch (state) {
        case ADDRESS:
          // Check if we got to the end of the address
          if (byte == '\0') {
            // Set the end of the address
            addressEnd = i;

            // Set the start of the type tag
            typeTagStart = i + 1;

            // increment
            i += 4;

            // Set the state to TYPE_TAG
            state = TYPE_TAG;
          } else {
            // Set the start of the address
            addressStart = i;

            // increment
            i += 4;
          }
          break;
        case TYPE_TAG:
          // Check if we got to the end of the type tag
          if (byte == '\0') {
            // Set the end of the type tag
            typeTagEnd = i;

            // Set the state to ARGUMENTS
            state = ARGUMENTS;

            // increment
            i += 4;
          } else {
            // Set the start of the type tag
            typeTagStart = i;

            // increment
            i += 4;
          }
          break;
        case ARGUMENTS:
          // Check the type tag
          if (buffer[i] == 'f') {
            // Get the float value
            float value = 0.f;
            std::memcpy(&value, buffer + i + 1, 4);

            // Swap the endianness of the value
            value = swap_endian<float>(value);

            // Add the value to the message
            add<float>(value);

            // Increment i by 4
            i += 4;
          } else if (buffer[i] == 'i') {
            // Get the int value
            int32_t value = 0;
            std::memcpy(&value, buffer + i + 1, 4);

            // Swap the endianness of the value
            value = swap_endian<int32_t>(value);

            // Add the value to the message
            add<int32_t>(value);

            // Increment i by 4
            i += 4;
          }
			  // else if (buffer[i] == 's') {
			  // // Parse const char * string
			  // const char * value = buffer + i + 1;
					//
			  // // Add the value to the message
			  // add<const char *>(value);
					//

                      // }
      };
    }

    // Print address
    // printf("Address: ");
    // for (int j = addressStart; j < addressEnd; j++) {
    //   printf("%c", buffer[j]);
    // };
    // printf("\n");
    //
    // // Print type
    //
    // printf("\nType tag: ");
    // for (int j = typeTagStart; j < typeTagEnd; j++) {
    //   printf("%c", buffer[j]);
    // };
    // printf("\n");
    //
    // // Print arguments
    // printf("\nArguments: ");
    // for (int j = typeTagEnd + 1; j < size; j++) {
    //   printf("%c", buffer[j]);
    // };
    // printf("\n");
  }

private:
  // Buffer
  std::size_t mBufferSize;

  // A buffer based on char as the basic type
  char mBuffer[MAX_MESSAGE_SIZE];
};

// A simple OSC server that can receive OSC messages using lwip
class OSCServer
{
public:
  // Constructor that takes a port number
  OSCServer(uint16_t serverPort)
  {
    // Create a new OSC pcb
    mPcb = udp_new();

#if NO_SYS == 1
    cyw43_arch_lwip_begin();
#endif
    err_t _bindErr = ERR_OK;

    if (mPcb != nullptr
        // (mPcb = udp_new_ip_type(IPADDR_TYPE_V4)) != NULL
    )
    {
      // Set receive callback
      setReceiveFunction();

      _bindErr = udp_bind(mPcb, IP_ADDR_ANY, serverPort);
    } else
      _bindErr = ERR_MEM;
#if NO_SYS == 1
    cyw43_arch_lwip_end();
#endif

    // Bind the pcb to the port
    // const auto result = udp_bind(mPcb, IP_ADDR_ANY, port);

    if (_bindErr != ERR_OK) {
      printf("Failed to bind server to UDP pcb! error=%d\n", _bindErr);
    } else {
      printf("Bound server to UDP pcb to port %d\n", serverPort);
    }

    // // Connect
    // const auto connError = udp_connect(mPcb, IP_ADDR_ANY, serverPort);
    //
    // if (connError != ERR_OK) {
    //   printf("Failed to connect UDP pcb! error=%d\n", connError);
    // } else {
    //   printf("Connected UDP pcb to port %d\n", serverPort);
    // };
  }

  // Destructor
  ~OSCServer()
  {
    // Close the pcb
    udp_remove(mPcb);
  }

  // Receive packet
  // Returns a an OSC message if a packet was received
  // and a nullptr if no packet was received
  void setReceiveFunction()
  {


    // Attempt to receive the packet
    udp_recv(
        mPcb,
        [](
            void* arg, udp_pcb* pcb, pbuf* p, const ip_addr_t* addr, u16_t port)
        {
          // Get ip of incoming packet
          const auto ip = ipaddr_ntoa(addr);

          // Get the length of the packet
          const auto len = p->len;

          if (p != nullptr) {
            // Get the payload of the packet
            const auto payload = reinterpret_cast<const char*>(p->payload);

            // Print
            printf(
                "Received packet from %s:%d with length %d\n", ip, port, len);

            //------------------------------------------------------------------//
            //                        Parse OSC message //
            //------------------------------------------------------------------//

            // Parse as OSC message
            OSCMessage message;
            message.parse(payload, len);
            message.print();
            const auto typeTag = message.getTypeTag();

            if (typeTag != nullptr) {
              printf("Type tag: %s\n", typeTag);

			  // print tag position
			  const auto typeTagPosition = message.findTypeTagPosition();
			  printf("Type tag position: %d\n", typeTagPosition);

              // print arguments
              const auto arguments = message.getArguments();

			  // print argument position
			  const auto argumentsPosition = message.findArgumentsPosition();
			  printf("Arguments position: %d\n", argumentsPosition);

              if (arguments != nullptr) {
                // Print arguments
                printf("Arguments: ");
                for (int i = 0; i < std::strlen(arguments); i++) {
                  printf("%c", arguments[i]);
                }
                printf("\n");

                // Print float
                const auto value = message.getInt();
                printf("Value: %d\n", value);
              }
            }

            // Free pbuffer
            pbuf_free(p);
          }
        },
        NULL);
  }

private:
  udp_pcb* mPcb;
};

}  // namespace picoosc
