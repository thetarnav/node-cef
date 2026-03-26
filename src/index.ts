import * as module from 'node:module'
import * as url    from 'node:url'
import * as path   from 'node:path'
import * as fs     from 'node:fs'

type Token = number & {__token: void}

type Init_Options = {
    browserSubprocessPath?: string
}

type Create_Window_Options = {
    width?: number
    height?: number
    windowless?: boolean
}

type Native_API = {
    init:         (opts: Init_Options) => void
    createWindow: (url: string, onMessage: (msg: string) => void, options?: Create_Window_Options) => Token
    send:         (token: Token, message: string) => void
    close:        (token: Token) => void
    shutdown:     () => void
}

let native: Native_API | null = null

export function init(options?: Init_Options): void {
    if (native !== null) {
        return
    }

    let require = module.createRequire(import.meta.url)
    let dirname = path.dirname(url.fileURLToPath(import.meta.url))

    let possible_paths = [
        path.join(dirname, '..', 'build', 'cef_bridge.node'),
        path.join(process.cwd(), 'build', 'cef_bridge.node'),
    ]

    for (let addon_path of possible_paths) {
        if (fs.existsSync(addon_path)) {
            native = require(addon_path)
            native!.init({
                browserSubprocessPath: options?.browserSubprocessPath,
            })
            return
        }
    }

    throw new Error(
        `Could not find cef_bridge.node addon.\n` +
        `Searched paths:\n${possible_paths.map(p => `  - ${p}`).join('\n')}\n\n` +
        `Make sure you have run 'bun run build:cef' and are using:\n` +
        `  LD_LIBRARY_PATH=build bun run example`
    )
}

export function createWindow(
    url: string,
    onMessage: (msg: string) => void,
    options?: Create_Window_Options
): Token {
    if (!native) {
        throw new Error('Call init() before createWindow()')
    }
    return native.createWindow(url, onMessage, options)
}

export function send(token: Token, message: string): void {
    if (!native) {
        throw new Error('Call init() before send()')
    }
    native.send(token, message)
}

export function close(token: Token): void {
    if (!native) {
        throw new Error('Call init() before close()')
    }
    native.close(token)
}

export function shutdown(): void {
    if (!native) {
        return
    }
    native.shutdown()
    native = null
}
