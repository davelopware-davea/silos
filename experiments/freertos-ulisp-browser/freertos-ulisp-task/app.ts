// This source intentionally stays within the JavaScript-compatible TypeScript
// subset so the spike does not need a package-manager toolchain.
const transcript = document.querySelector('#transcript');
const form = document.querySelector('#repl');
const input = document.querySelector('#expression');
const runButton = document.querySelector('#run');
const startButton = document.querySelector('#start');
const pauseButton = document.querySelector('#pause');
const resetButton = document.querySelector('#reset');
const stepButton = document.querySelector('#step');
const stateValue = document.querySelector('#state-value');
const tickValue = document.querySelector('#tick-value');
const timeValue = document.querySelector('#time-value');
const yieldValue = document.querySelector('#yield-value');
const heapValue = document.querySelector('#heap-value');
const displayValue = document.querySelector('#display-value');
const canvas = document.querySelector('#display');
const context = canvas.getContext('2d');

let worker;
let nextRequestId = 1;
let ready = false;
let paused = false;

function append(text, className = '') {
  const line = document.createElement('span');
  line.className = className;
  line.textContent = text;
  transcript.append(line);
  transcript.scrollTop = transcript.scrollHeight;
}

function setControls() {
  input.disabled = !ready;
  runButton.disabled = !ready;
  startButton.disabled = !ready || !paused;
  pauseButton.disabled = !ready || paused;
  stepButton.disabled = !ready || !paused;
  stateValue.textContent = !ready ? 'starting' : paused ? 'paused' : 'running';
}

function drawDisplay(message) {
  const image = context.createImageData(128, 64);
  let litPixels = 0;
  for (let bit = 0; bit < 128 * 64; bit += 1) {
    const on = (message.pixels[bit >> 3] & (1 << (bit & 7))) !== 0;
    const offset = bit * 4;
    image.data[offset] = on ? 141 : 7;
    image.data[offset + 1] = on ? 255 : 17;
    image.data[offset + 2] = on ? 173 : 11;
    image.data[offset + 3] = 255;
    if (on) litPixels += 1;
  }
  context.putImageData(image, 0, 0);
  canvas.dataset.revision = String(message.revision);
  canvas.dataset.litPixels = String(litPixels);
  displayValue.textContent = `${litPixels} pixels / rev ${message.revision}`;
}

function startWorker() {
  if (worker) worker.terminate();
  ready = false;
  paused = false;
  transcript.textContent = '';
  append('Starting worker runtime...\n', 'muted');
  context.fillStyle = '#07110b';
  context.fillRect(0, 0, 128, 64);
  delete canvas.dataset.revision;
  delete canvas.dataset.litPixels;
  displayValue.textContent = '0 pixels / rev 0';
  setControls();

  worker = new Worker('worker.js');
  worker.onmessage = event => {
    const message = event.data;
    if (message.type === 'ready') {
      ready = true;
      append('FreeRTOS + uLisp worker ready.\n', 'system');
      setControls();
    } else if (message.type === 'status') {
      paused = message.paused;
      tickValue.textContent = String(message.tick);
      setControls();
    } else if (message.type === 'result') {
      append(message.output, 'result');
      tickValue.textContent = String(message.tick);
      timeValue.textContent = `${message.elapsedMs.toFixed(1)} ms`;
      yieldValue.textContent = String(message.safePointYields);
      heapValue.textContent = `${Math.round(message.freeHeapBytes / 1024)} KiB`;
    } else if (message.type === 'display') {
      drawDisplay(message);
    } else if (message.type === 'log') {
      append(`${message.text}\n`, message.stream === 'stderr' ? 'error' : 'muted');
    }
  };
  worker.onerror = event => {
    append(`Worker error: ${event.message}\n`, 'error');
    ready = false;
    setControls();
  };
}

form.addEventListener('submit', event => {
  event.preventDefault();
  const expression = input.value.trim();
  if (!expression || !ready) return;
  append(`> ${expression}\n`, 'prompt');
  worker.postMessage({ type: 'evaluate', id: nextRequestId++, expression });
  input.select();
});

startButton.addEventListener('click', () => worker.postMessage({ type: 'control', action: 'resume' }));
pauseButton.addEventListener('click', () => worker.postMessage({ type: 'control', action: 'pause' }));
stepButton.addEventListener('click', () => worker.postMessage({ type: 'control', action: 'step' }));
resetButton.addEventListener('click', startWorker);

startWorker();
