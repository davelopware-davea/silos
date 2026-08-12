const runButton = document.querySelector('#run');
const downloadButton = document.querySelector('#download');
const status = document.querySelector('#status');
const output = document.querySelector('#output');

const cases = [
  { name: 'warmup', repetitions: 1, expression: '(+ 1 1)' },
  { name: 'short-arithmetic', repetitions: 10, expression: '(+ 20 22)' },
  { name: 'long-evaluation', repetitions: 3, expression: '(let ((x 0)) (dotimes (i 100000 x) (setq x (+ x 1))))' },
  { name: 'allocation-pressure', repetitions: 3, expression: '(dotimes (i 20000 i) (list i i i i))' },
  { name: 'explicit-gc', repetitions: 5, expression: '(gc)' }
];

let latestResult = null;
let runSequence = 0;
const runtimeName = new URLSearchParams(location.search).get('runtime') === 'release'
  ? 'release'
  : 'development';

async function byteLength(url) {
  const response = await fetch(url, { cache: 'no-store' });
  if (!response.ok) throw new Error(`${url}: HTTP ${response.status}`);
  return (await response.arrayBuffer()).byteLength;
}

function runWorkerBenchmark() {
  return new Promise((resolve, reject) => {
    const startedAt = performance.now();
    const worker = new Worker(runtimeName === 'release'
      ? 'worker.js?runtime=release'
      : 'worker.js');
    const samples = [];
    let requestId = 0;
    let caseIndex = 0;
    let repetition = 0;
    let requestStartedAt = 0;
    let readyMs = null;
    let navigationToReadyMs = null;

    function sendNext() {
      if (caseIndex >= cases.length) {
        worker.terminate();
        resolve({
          readyMs,
          navigationToReadyMs,
          visibilityState: document.visibilityState,
          samples
        });
        return;
      }
      const benchmarkCase = cases[caseIndex];
      status.textContent = `${benchmarkCase.name} ${repetition + 1}/${benchmarkCase.repetitions}`;
      requestStartedAt = performance.now();
      worker.postMessage({ type: 'evaluate', id: ++requestId, expression: benchmarkCase.expression });
    }

    worker.onerror = event => {
      worker.terminate();
      reject(new Error(event.message || 'Worker failed'));
    };

    worker.onmessage = event => {
      const message = event.data;
      if (message.type === 'ready') {
        readyMs = performance.now() - startedAt;
        navigationToReadyMs = performance.now();
        sendNext();
        return;
      }
      if (message.type !== 'result') return;

      const benchmarkCase = cases[caseIndex];
      samples.push({
        case: benchmarkCase.name,
        repetition: repetition + 1,
        output: message.output.trim(),
        roundTripMs: performance.now() - requestStartedAt,
        workerElapsedMs: message.elapsedMs,
        kernelTick: message.tick,
        startTick: message.startTick,
        tickDelta: message.tickDelta,
        safePointYields: message.safePointYields,
        maxUninterruptedMs: message.maxUninterruptedMs,
        intervalsOver100Ms: message.intervalsOver100Ms,
        intervalsOver250Ms: message.intervalsOver250Ms,
        freeHeapBytes: message.freeHeapBytes,
        minimumFreeHeapBytes: message.minimumFreeHeapBytes,
        freeRtosHeapCapacityBytes: message.freeRtosHeapCapacityBytes,
        ulispWorkspaceBytes: message.ulispWorkspaceBytes,
        ulispFreeBytes: message.ulispFreeBytes,
        wasmMemoryBytes: message.wasmMemoryBytes,
        wasmDynamicTopBytes: message.wasmDynamicTopBytes,
        gcCount: message.gcCount,
        gcTotalMs: message.gcTotalMs,
        gcMaxMs: message.gcMaxMs,
        maxTickLatenessMs: message.maxTickLatenessMs,
        schedulerPasses: message.schedulerPasses,
        tickCatchupEvents: message.tickCatchupEvents,
        maxTicksPerPass: message.maxTicksPerPass
      });

      repetition += 1;
      if (repetition >= benchmarkCase.repetitions) {
        repetition = 0;
        caseIndex += 1;
      }
      sendNext();
    };
  });
}

async function run() {
  runButton.disabled = true;
  downloadButton.disabled = true;
  latestResult = null;
  output.textContent = '';
  status.textContent = 'measuring artifacts';

  try {
    const [javascriptBytes, wasmBytes, releaseJavaScriptBytes, releaseWasmBytes] = await Promise.all([
      byteLength('freewisp-freertos-ulisp-worker.js'),
      byteLength('freewisp-freertos-ulisp-worker.wasm'),
      byteLength('freewisp-freertos-ulisp-worker-release.js'),
      byteLength('freewisp-freertos-ulisp-worker-release.wasm')
    ]);
    runSequence += 1;
    const runtime = await runWorkerBenchmark();
    latestResult = {
      schemaVersion: 2,
      measuredAt: new Date().toISOString(),
      userAgent: navigator.userAgent,
      timingProtocol: {
        runSequence,
        runKind: runSequence === 1 ? 'fresh-page-worker' : 'warm-worker',
        runtime: runtimeName,
        visibilityState: runtime.visibilityState
      },
      artifacts: {
        development: { javascriptBytes, wasmBytes },
        release: { javascriptBytes: releaseJavaScriptBytes, wasmBytes: releaseWasmBytes }
      },
      workload: cases,
      startup: {
        workerReadyMs: runtime.readyMs,
        navigationToWorkerReadyMs: runSequence === 1
          ? runtime.navigationToReadyMs
          : null
      },
      samples: runtime.samples
    };
    output.textContent = JSON.stringify(latestResult, null, 2);
    status.textContent = `complete: ${runtime.samples.length} samples`;
    downloadButton.disabled = false;
  } catch (error) {
    status.textContent = 'failed';
    output.textContent = error instanceof Error ? error.stack : String(error);
  } finally {
    runButton.disabled = false;
  }
}

runButton.addEventListener('click', run);
downloadButton.addEventListener('click', () => {
  if (!latestResult) return;
  const blob = new Blob([`${JSON.stringify(latestResult, null, 2)}\n`], { type: 'application/json' });
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = `freewisp-benchmark-${latestResult.measuredAt.replaceAll(':', '-')}.json`;
  link.click();
  URL.revokeObjectURL(link.href);
});

run();
