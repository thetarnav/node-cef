# AGENTS Guide

This file is for coding agents working in this repository.
It documents practical conventions observed in the codebase.

## Project Structure

```
bun-cef/
├── src/
│   ├── index.ts              # TypeScript library wrapper (lazy loads addon)
│   ├── cef_bridge.cc         # C++ addon + helper executable (single file)
│   └── tsconfig.json         # TypeScript config for library
├── build/                    # Build output
│   ├── cef_bridge.node       # Native addon (.node)
│   ├── cef_bridge_helper     # Browser subprocess executable
│   ├── libcef.so             # CEF shared library
│   ├── Resources/            # ICU data, locales, pak files
│   └── *.so                  # GPU/Vulkan libraries
├── cef/linux64/              # Downloaded CEF binaries (reference)
├── example/
│   ├── index.ts              # Example usage
│   ├── index.html            # Test HTML page
│   └── tsconfig.json
├── CMakeLists.txt            # C++ build configuration
├── download-cef.ts           # Script to download CEF
├── package.json
└── README.md
```

## Architecture

The library uses a Node.js addon approach:
- **TypeScript wrapper** (`src/index.ts`) - lazy loads the native addon
- **C++ addon** (`src/cef_bridge.cc`) - builds as both `.node` addon and helper executable
- **CEF integration** - links against CEF shared library, runs browser in separate process

### Key Design Decisions

1. **Lazy addon loading** - `init()` must be called before other functions; addon is loaded on first `init()` call
2. **Branded Token type** - `Token = number & {__token: void}` prevents accidental number usage
3. **Message routing** - browser_id to token mapping for multi-window support
4. **Thread-safe callbacks** - NAPI thread-safe functions for JS↔C++ communication
5. **V8 bridge** - exposes `window.cef.send()` and `window.cef.onmessage` in renderer

### C++ to TypeScript Mapping

| C++ Component | Purpose |
|--------------|---------|
| `Bridge_Client` | Browser lifecycle, message handling |
| `Bridge_Render_Process_Handler` | V8 context, injects cef object |
| `Native_Send_Handler` | Handles `cef.send()` in renderer |
| `tsfn_call_js` | Calls JS callback from CEF thread |

## Import and Module Conventions

- Use ESM imports only.
- Prefer namespace imports for local modules:
  `import * as cef from './cef.ts'`
- Use explicit `.ts` extension in local imports
- Node builtins are imported as `node:*` modules
- Avoid default exports; project uses named exports

## Naming Conventions

- Functions/variables: `snake_case` (for example `token_next`, `parse_src`)
- Types/classes: `Ada_Case` with underscores (for example `Token_Kind`, `Node_World`)
- Constants: `SCREAMING_SNAKE_CASE` (`TOKEN_EOF`, `MAX_ID`)
- Exported aliases may coexist for compatibility; do not remove them casually

## C++ Conventions

- **Single source file** - all C++ code in `src/cef_bridge.cc`
- **Conditional compilation** - `#ifdef CEF_HELPER_BINARY` for helper vs addon
- **CEF headers** - include from `cef/linux64/include/`
- **No exceptions in helper** - built with `-fno-exceptions -fno-rtti`
- **Addons use exceptions** - built with `-fexceptions -frtti`

## Build Commands

```bash
# Download CEF binaries
bun run download:cef

# Build C++ addon and helper
bun run build:cef

# Or combined
bun run setup

# TypeScript
bun run build        # Build declarations
bun run typecheck    # Type check
```

## Testing

```bash
# Run example (requires LD_LIBRARY_PATH)
bun run example

# Or manually:
cd build && LD_LIBRARY_PATH=. bun run ../example/index.ts
```

## Change Management for Agents

- Make minimal diffs; avoid unrelated cleanup
- If touching complex logic, add or update tests in the same change
- Preserve backward-compatible exported APIs unless explicitly changing them
- When behavior changes, update `readme.md` examples if needed
- Update `TODO.md` with architecture changes

## Quick Pre-Handoff Checklist

- `bun run build:cef` works
- `bun run example` works
- no accidental dependency additions
- no unrelated file reformatting
- docs (AGENTS.md, README.md, TODO.md) updated if API changed
