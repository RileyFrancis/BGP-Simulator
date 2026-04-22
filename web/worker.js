importScripts('bgp_sim.js');

let Module = null;

createBGPModule({
    print:    function() {},   // suppress stdout from C++ in browser console
    printErr: function() {}    // suppress stderr
}).then(function(m) {
    Module = m;
    postMessage({ type: 'ready' });
});

// Allocate a JS string on the WASM heap and return its pointer.
// The caller is responsible for calling Module._free(ptr) when done.
// (ccall's built-in 'string' type uses stackAlloc, which overflows for
//  multi-megabyte inputs like the full CAIDA topology file.)
function heapString(str) {
    const len = Module.lengthBytesUTF8(str) + 1;
    const ptr = Module._malloc(len);
    if (!ptr) throw new Error('WASM malloc failed — out of memory');
    Module.stringToUTF8(str, ptr, len);
    return ptr;
}

self.onmessage = function(e) {
    if (e.data.type !== 'simulate') return;

    const { caida, anns, rov, targetAsn } = e.data;

    let caidaPtr = 0, annsPtr = 0, rovPtr = 0;
    try {
        // Heap-allocate all three input strings before calling into C++
        caidaPtr = heapString(caida);
        annsPtr  = heapString(anns);
        rovPtr   = heapString(rov || '');

        const result = Module.ccall(
            'run_bgp_simulation',
            'string',                               // return: read char* immediately as JS string
            ['number', 'number', 'number', 'number'],
            [caidaPtr, annsPtr, rovPtr, targetAsn >>> 0]
        );
        postMessage({ type: 'result', data: result });

    } catch (err) {
        postMessage({ type: 'error', message: String(err) });
    } finally {
        // Free heap allocations regardless of success or failure
        if (caidaPtr) Module._free(caidaPtr);
        if (annsPtr)  Module._free(annsPtr);
        if (rovPtr)   Module._free(rovPtr);
    }
};
