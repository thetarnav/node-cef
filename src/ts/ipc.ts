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
