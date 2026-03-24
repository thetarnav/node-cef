import { IPCServer } from './ipc.js';

export class Window {
	private id: string;
	private ipc: IPCServer;
	private messageHandlers: ((msg: string) => void)[] = [];

	constructor(id: string, ipc: IPCServer) {
		this.id = id;
		this.ipc = ipc;
	}

	postMessage(msg: string) {
		this.ipc.sendToAll(`window:post:${this.id}:${msg}`);
	}

	onMessage(callback: (msg: string) => void) {
		this.messageHandlers.push(callback);
	}

	handleIncomingMessage(msg: string) {
		for (const handler of this.messageHandlers) {
			handler(msg);
		}
	}

	close() {
		this.ipc.sendToAll(`window:close:${this.id}`);
	}
}
