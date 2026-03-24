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
	// more options can be added later
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