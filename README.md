# node-cef

A lightweight Node/Bun/Deno library for creating embedded browser windows with bidirectional communication between client JavaScript and the backend.

## Features

- Create browser windows with a simple API
- Send messages from the backend to browser renderer
- Receive messages from browser renderer in the backend
- Multiple window support via tokens (window id)

## Installation

```bash
npm install node-cef
```

~This will download CEF binaries and build the native addon.~

## Quick Start

```typescript
import * as cef from 'node-cef'

// Initialize the library (call once at startup)
cef.init()

// Create a browser window
const token = cef.createWindow('https://example.com', (msg: string) => {
    console.log('Message from browser:', msg)
    // Echo back to browser
    cef.send(token, 'echo: ' + msg)
})

// Send a message to the browser
cef.send(token, 'Hello from Bun!')

// Close the window when done
cef.close(token)

// Shutdown on exit
cef.shutdown()
```

## Browser API

In the renderer (HTML/JS), use the `cef` object:

```html
<script>
    // Send a message to Bun
    window.cef.send('Hello from browser!')

    // Receive messages from Bun
    window.cef.onmessage = function(msg) {
        console.log('Received from Bun:', msg)
    }
</script>
```

## Running the Example

```bash
bun run example
```

The addon uses RPATH to find CEF libraries in the build directory, so no LD_LIBRARY_PATH needed.

## Required Dependencies

The library requires CEF shared libraries. These are automatically downloaded during installation and copied to the `build/` directory.

### Dynamic Libraries

The following libraries must be accessible at runtime:

- `libcef.so` - CEF core
- `libEGL.so` - OpenGL
- `libGLESv2.so` - OpenGL ES
- `libvk_swiftshader.so` - Vulkan/GLES
- `icudtl.dat` - ICU data

These are located in the `build/` directory. RPATH is set to `$ORIGIN` so libraries are found automatically.

## Developer Setup

### Prerequisites

- Bun
- CMake
- C++ compiler (gcc/clang)
- make

### Building

```bash
# Download CEF binaries
bun run download:cef

# Build the C++ addon
bun run build:cef

# Or build everything
bun run setup
```

### Running Tests

```bash
bun run example
```

## Project Structure

```
bun-cef/
├── src/
│   ├── index.ts         # TypeScript wrapper
│   └── cef_bridge.cc    # C++ addon + helper
├── build/               # Built binaries and CEF libs
├── example/             # Example app
└── cef/                 # CEF source binaries
```

## License

MIT
