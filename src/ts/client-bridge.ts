// Client-side JavaScript bridge to be injected into HTML pages
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