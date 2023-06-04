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

    // Iterate over all bytes in the input buffer and parse the message
	// use the addAddress() method to add the address to this message's buffer
	// use the add() method to add the arguments to this message's buffer
    while (i < size) {


    }
  }

  // Find position of type tag in buffer
  int findTypeTagPosition() const
  {
    // Get the position of the type tag
    int typeTagPosition = 0;
    for (int i = 0; i < mBufferSize; i++) {
      if (mBuffer[i] == ',') {
        typeTagPosition = i;
      }
    }
    return typeTagPosition;
  }

  // Get type tag
  const char* getTypeTag() const
  {
    // Get the position of the type tag
    const auto typeTagPosition = findTypeTagPosition();

    if (typeTagPosition == 0) {
      return nullptr;
    }

    // Get the type tag from the buffer
    const char* typeTag = mBuffer + typeTagPosition + 1;

    return typeTag;
  }

  // Get position of arguments in buffer
  int findArgumentsPosition() const
  {
    // Get the position of the type tag
    const auto typeTagPosition = findTypeTagPosition();

    const auto typeTagLength = std::strlen(getTypeTag());

    // Get the position of the arguments
    const auto argumentsPosition = typeTagPosition + typeTagLength + 1;

    return argumentsPosition;
  }

  // Get arguments
  const char* getArguments() const
  {
    // Get the position of the arguments
    const auto argumentsPosition = findArgumentsPosition();

    if (argumentsPosition == 0) {
      return nullptr;
    }

    // Get the arguments from the buffer
    const char* arguments = mBuffer + argumentsPosition;

    return arguments;
  }

  // Get integer from buffer
  int32_t getInt()
  {
    // Get the position of the arguments
    const auto argumentsPosition = findArgumentsPosition();

    // Get the int value from the first position
    int32_t value = 0;
    const auto numBytes = sizeof(int32_t);

    std::memcpy(&value, mBuffer + argumentsPosition, numBytes);

    // Swap the endianness of the value
    // value = swap_endian<int32_t>(value);

    return value;
  }

  /*
  decodeData(uint8_t incomingByte){
//get the first OSCData to re-set
for (int i = 0; i < dataCount; i++){
  OSCData * datum = getOSCData(i);
  if (datum->error == INVALID_OSC){
      //set the contents of datum with the data received
      switch (datum->type){
          case 'i':
              if (incomingBufferSize == 4){
                  //parse the buffer as an int
                  union {
                      int32_t i;
                      uint8_t b[4];
                  } u;
                  memcpy(u.b, incomingBuffer, 4);
                  int32_t dataVal = BigEndian(u.i);
                  set(i, dataVal);
                  clearIncomingBuffer();
              }
              break;
          case 'f':
              if (incomingBufferSize == 4){
                  //parse the buffer as a float
                  union {
                      float f;
                      uint8_t b[4];
                  } u;
                  memcpy(u.b, incomingBuffer, 4);
                  float dataVal = BigEndian(u.f);
                  set(i, dataVal);
                  clearIncomingBuffer();
              }
              break;
          case 'd':
              if (incomingBufferSize == 8){
                  //parse the buffer as a double
                  union {
                      double d;
                      uint8_t b[8];
                  } u;
                  memcpy(u.b, incomingBuffer, 8);
                  double dataVal = BigEndian(u.d);
                  set(i, dataVal);
                  clearIncomingBuffer();
              }
              break;
          case 't':
              if (incomingBufferSize == 8){
                  //parse the buffer as a timetag
                  union {
                      osctime_t t;
                      uint8_t b[8];
                  } u;
                  memcpy(u.b, incomingBuffer, 8);

                  u.t.seconds = BigEndian(u.t.seconds);
                  u.t.fractionofseconds = BigEndian(u.t.fractionofseconds);
                  set(i, u.t);
                  clearIncomingBuffer();
              }
              break;

          case 's':
              if (incomingByte == 0){
                  char * str = (char *) incomingBuffer;
                  set(i, str);
                  clearIncomingBuffer();
                  decodeState = DATA_PADDING;
              }
              break;
          case 'b':
              if (incomingBufferSize > 4){
                  //compute the expected blob size
                  union {
                      uint32_t i;
                      uint8_t b[4];
                  } u;
                  memcpy(u.b, incomingBuffer, 4);
                  uint32_t blobLength = BigEndian(u.i);
                  if (incomingBufferSize == (int)(blobLength + 4)){
                      set(i, incomingBuffer + 4, blobLength);
                      clearIncomingBuffer();
                      decodeState = DATA_PADDING;
                  }

              }
              break;
      }
      //break out of the for loop once we've selected the first invalid message
      break;
  }
}
}

           */
  // A decode function based on the above method from OSCMessage.h in CNMAT's
  // OSC library

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
        [](void* arg, udp_pcb* pcb, pbuf* p, const ip_addr_t* addr, u16_t port)
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
