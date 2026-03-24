import type { WindowOptions } from './types.js';
import { IPCServer } from './ipc.js';
import { Window } from './window.js';

export class App {
	private ipc: IPCServer;
	private windows = new Map<string, Window>();
	private cefProcess: any = null;
	private ready = false;
	private readyHandlers: (() => void)[] = [];
	private pendingWindows: { opts: WindowOptions; resolve: (w: Window) => void }[] = [];

	constructor() {
		this.ipc = new IPCServer();
	}

	async start() {
		await this.ipc.start();
		this.spawnCEF();
		this.setupMessageHandlers();
	}

	private spawnCEF() {
		const binaryPath = process.cwd() + '/src/cpp/build/cef_host';
		const binaryDir = process.cwd() + '/src/cpp/build';
		
		const socketPath = this.ipc.getSocketPath();
		this.cefProcess = Bun.spawn([binaryPath, '--socket-path=' + socketPath, '--no-auto-browser'], {
			stdio: ['ignore', 'inherit', 'inherit'],
			cwd: binaryDir,
		});
	}

	private setupMessageHandlers() {
		this.ipc.onMessage((msg: string) => {
			if (msg === 'hello') {
				this.ready = true;
				this.readyHandlers.forEach(h => h());
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
					if (win) win.handleIncomingMessage(message);
				}
				return;
			}
		});
	}

	private processPendingWindows() {
		for (const { opts, resolve } of this.pendingWindows) {
			const win = this.doCreateWindow(opts);
			resolve(win);
		}
		this.pendingWindows = [];
	}

	onReady(callback: () => void) {
		if (this.ready) callback();
		else this.readyHandlers.push(callback);
	}

	async createWindow(options: WindowOptions): Promise<Window> {
		if (this.ready) {
			return this.doCreateWindow(options);
		}
		return new Promise(resolve => {
			this.pendingWindows.push({ opts: options, resolve });
		});
	}

	private doCreateWindow(options: WindowOptions): Window {
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
