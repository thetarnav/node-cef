import { IPCServer } from './ipc.js';

export class Window {
	id: string;
	ipc: IPCServer;
  onMessage: ((msg: string) => void) | undefined
  onClose: (() => void) | undefined

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
