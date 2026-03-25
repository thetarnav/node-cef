export type Message = {
    id?: string;
    type: string;
    payload?: any;
};

export type WindowOptions = {
    html?: string;
    url?: string;
    width?: number;
    height?: number;
    devtools?: boolean;
    hidden?: boolean;
    frameless?: boolean;
    icon?: string;
    title?: string;
};

export type CreateWindowMessage = {
    type: 'window:create';
    payload: WindowOptions & { id: string };
};

export type WindowPostMessage = {
    type: 'window:post';
    payload: { id: string; msg: any };
};

export type WindowDevToolsMessage = {
    type: 'window:devtools';
    payload: { id: string; open: boolean };
};

export type WindowCloseMessage = {
    type: 'window:close';
    payload: { id: string };
};

export type AppQuitMessage = {
    type: 'app:quit';
};

export type AnyMessage = CreateWindowMessage | WindowPostMessage | WindowDevToolsMessage | WindowCloseMessage | AppQuitMessage | Message;

export class IPCServer {
    private socketPath: string;
    private server: any | null = null;
    private clients = new Set<any>();
    private messageHandlers: ((msg: string, socket: any) => void)[] = [];

    constructor(socketPath: string = '/tmp/cef.sock') {
        this.socketPath = socketPath;
    }

    getSocketPath(): string {
        return this.socketPath;
    }

    async start(): Promise<void> {
        this.server = Bun.listen({
            unix: this.socketPath,
            socket: {
                open: (socket: any) => {
                    this.clients.add(socket);
                },
                data: (socket: any, data: Buffer) => {
                    const lines = data.toString().split('\n');
                    for (const line of lines) {
                        if (line.trim()) {
                            for (const handler of this.messageHandlers) {
                                handler(line, socket);
                            }
                        }
                    }
                },
                close: (socket: any) => {
                    this.clients.delete(socket);
                },
                error: () => {},
            },
        });
    }

    onMessage(handler: (msg: string, socket: any) => void) {
        this.messageHandlers.push(handler);
    }

    sendToAll(msg: string) {
        const line = msg + '\n';
        for (const client of this.clients) {
            client.write(line);
        }
    }

    sendToSocket(socket: any, msg: string) {
        socket.write(msg + '\n');
    }

    close() {
        this.server?.stop();
        this.clients.clear();
    }
}

export class Window {
    id: string;
    ipc: IPCServer;
    onMessage: ((msg: string) => void) | undefined;
    onClose: (() => void) | undefined;

    constructor(id: string, ipc: IPCServer) {
        this.id = id;
        this.ipc = ipc;
    }

    postMessage(msg: string) {
        this.ipc.sendToAll(`window:post:${this.id}:${msg}`);
    }

    close() {
        this.ipc.sendToAll(`window:close:${this.id}`);
    }
}

export class App {
    ipc: IPCServer;
    windows = new Map<string, Window>();
    cefProcess: any = null;
    ready = false;
    pendingWindows: { opts: WindowOptions; resolve: (w: Window) => void }[] = [];
    onReady: (() => void) | undefined;

    constructor() {
        this.ipc = new IPCServer();
    }

    async start() {
        await this.ipc.start();
        this.spawnCEF();
        this.setupMessageHandlers();
    }

    spawnCEF() {
        const binaryPath = process.cwd() + '/build/cef_host';
        const binaryDir = process.cwd() + '/build';

        const socketPath = this.ipc.getSocketPath();
        this.cefProcess = Bun.spawn([binaryPath, '--socket-path=' + socketPath, '--no-auto-browser'], {
            stdio: ['ignore', 'inherit', 'inherit'],
            cwd: binaryDir,
        });
    }

    setupMessageHandlers() {
        this.ipc.onMessage((msg: string) => {
            if (msg === 'hello') {
                this.ready = true;
                this.onReady?.();
                this.processPendingWindows();
                return;
            }

            if (msg.startsWith('window:created:')) {
                const id = msg.substring(15);
                console.log('Window created:', id);
                return;
            }

            if (msg.startsWith('window:closed:')) {
                const id = msg.substring(14);
                console.log('Window closed:', id);
                let window = this.windows.get(id);
                console.assert(window != null);
                window?.onClose?.();
                this.windows.delete(id);
                return;
            }

            if (msg.startsWith('ui:event:')) {
                const rest = msg.substring(9);
                const colonIdx = rest.indexOf(':');
                if (colonIdx > 0) {
                    const windowId = rest.substring(0, colonIdx);
                    const message = rest.substring(colonIdx + 1);
                    const win = this.windows.get(windowId);
                    if (win) win.onMessage?.(message);
                }
                return;
            }
        });
    }

    processPendingWindows() {
        for (const { opts, resolve } of this.pendingWindows) {
            const win = this.doCreateWindow(opts);
            resolve(win);
        }
        this.pendingWindows = [];
    }

    async createWindow(options: WindowOptions): Promise<Window> {
        if (this.ready) {
            return this.doCreateWindow(options);
        }
        return new Promise(resolve => {
            this.pendingWindows.push({ opts: options, resolve });
        });
    }

    doCreateWindow(options: WindowOptions): Window {
        const id = `win_${Date.now()}`;
        const win = new Window(id, this.ipc);
        this.windows.set(id, win);

        let url = options.url || 'about:blank';
        if (options.html) {
            url = 'data:text/html,' + encodeURIComponent(options.html);
        }

        this.ipc.sendToAll(`window:create:${id}:${url}`);
        return win;
    }

    quit() {
        this.ipc.sendToAll('app:quit');
        setTimeout(() => {
            this.cefProcess?.kill();
            this.ipc.close();
            process.exit(0);
        }, 100);
    }
}

export const clientBridgeJS = `
(function() {
    window.sendToNative = function(data) {
        if (typeof data !== 'string') {
            console.warn('sendToNative expects a string argument');
            return;
        }
        if (window.sendToNative) {
            try {
                window.sendToNative(data);
            } catch(e) {
                console.warn('sendToNative failed:', e);
            }
        } else {
            console.warn('sendToNative not available');
        }
    };

    window.onMessage = null;

    window.addEventListener('cef-bridge-ready', function() {
        console.log('CEF bridge ready');
    });
})();
`;

export function createApp(): App {
    return new App();
}