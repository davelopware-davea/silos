globalThis.freewispRequests = [];
globalThis.freewispControls = [];

self.onmessage = event => {
  const message = event.data;
  if (!message || typeof message.type !== 'string') return;

  if (message.type === 'evaluate' &&
      Number.isInteger(message.id) &&
      typeof message.expression === 'string') {
    globalThis.freewispRequests.push({ id: message.id, expression: message.expression });
    return;
  }

  if (message.type === 'control' &&
      ['pause', 'resume', 'step'].includes(message.action)) {
    globalThis.freewispControls.push(message.action);
  }
};

var Module = {
  print(text) { postMessage({ type: 'log', stream: 'stdout', text }); },
  printErr(text) { postMessage({ type: 'log', stream: 'stderr', text }); }
};

importScripts('freewisp-freertos-ulisp-worker.js');
