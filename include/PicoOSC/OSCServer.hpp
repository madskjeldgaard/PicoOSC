#pragma once
// #include "OSCMessage.hpp"
#include "OSCMessageParser.hpp"
#include "lwip/udp.h"
#include "pico/cyw43_arch.h"

namespace picoosc
{
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


			OSCMessageParser::parse(payload, len);
            //------------------------------------------------------------------//
            //                        Parse OSC message //
            //------------------------------------------------------------------//

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
