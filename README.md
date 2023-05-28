# PicoOSC

A simple library for OSC communication on the Raspberry Pi Pico using the Raspberry Pi Pico C++ SDK.

It uses the [lwip](https://github.com/lwip-tcpip/lwip) library that is included with the Pico SDK.

## Support

This library is still a work in progress. Feel free to contribute to make it fully compatible with the whole [osc specification](https://opensoundcontrol.stanford.edu/spec-1_0.html) !

It currently supports:

- ✅ An OSC client interface:
    - ❌ multiple types in one message
    - OSC Message types:
        - ✅ string
        - ✅ float
        - ✅ int
        - ❌ blob
        - ❌ time tag
- ❌ An OSC server interface
- ❌ Bundles

## Usage example

### Sending an integer

```cpp
// Setup UDP target
const auto target = "192.168.0.105";
const auto port = 3333;

picoosc::OSCClient client(target, port);

// Make an integer message
picoosc::OSCMessage msg;

msg.addAddress("/picoint");
msg.add<int32_t>(8); 

// Send the message
msg.send(client);
```

## Adding this library to your project

### Using CPM
The easiest way to use this in your project is to install [it using CPM](https://github.com/cpm-cmake/CPM.cmake). Add the following lines to your CMakeLists.txt. This will automatically download and include it in your CMake setup.

```cmake
# Add Debounce
CPMAddPackage("gh:madskjeldgaard/PicoOSC#main")
target_link_libraries(SimplePicoMidiController PicoOSC)
```

### Manually
If you have a project called `SimplePicoMidiController`, you can link this library in your CMakeLists.txt like so (assuming you downloaded the library and placed it in the path noted below):  

```cmake
# Add Debounce
set(PICO_OSC_PATH "include/PicoOSC")
add_subdirectory(${PICO_OSC_PATH})
target_link_libraries(SimplePicoMidiController PicoOSC)
```
