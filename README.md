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

This requires `LD_LIBRARY_PATH=build` to find the CEF shared libraries. The package.json `example` script handles this automatically.

## Required Dependencies

The library requires CEF shared libraries. These are automatically downloaded during installation and copied to the `build/` directory.

### Dynamic Libraries

The following libraries must be accessible at runtime:

- `libcef.so` - CEF core
- `libEGL.so` - OpenGL
- `libGLESv2.so` - OpenGL ES
- `libvk_swiftshader.so` - Vulkan/GLES
- `icudtl.dat` - ICU data

These are located in the `build/` directory.

### Setting LD_LIBRARY_PATH

When running your application, ensure the build directory is in the library path:

```bash
LD_LIBRARY_PATH=build bun run your-app.ts
```

Or set it programmatically in your scripts.

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
