#pragma once
#include "OSCMessage.hpp"
#include "lwip/udp.h"

namespace picoosc
{

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

}  // namespace picoosc
