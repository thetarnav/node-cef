import { App } from './app.js';

export function createApp(): App {
	return new App();
}

export type { WindowOptions } from './types.js';
export { Window } from './window.js';
export { App } from './app.js';