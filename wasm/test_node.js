async function main() {
    const H264bsd = require('./h264bsd_wasm');
    const module = await H264bsd();
    const H264bsdDecoder = require('./h264bsd_decoder');

    // Load input data
    const util = require('util');
    const fs = require('fs');
    const readFile = util.promisify(fs.readFile);
    const data = await readFile('../test/test_1920x1080.h264');

    // Create a decoder instance
    var decoder = new H264bsdDecoder(module);
    decoder.onPictureReady = onPictureReady;
    decoder.onHeadersReady = onHeadersReady;
    decoder.queueInput(data);

    var pictureCount = 0;

    function onPictureReady() {
        ++pictureCount;
    }

    function onHeadersReady() {
        var width = decoder.outputPictureWidth();
        var height = decoder.outputPictureHeight();
        var croppingParams = decoder.croppingParams();
    
        console.log("Headers parsed", {
            'width' : width,
            'height' : height,
            'croppingParams' : croppingParams,
        });
    }

    var result = H264bsdDecoder.RDY;
    var start = process.hrtime();

    while(result != H264bsdDecoder.NO_INPUT) {
        result = decoder.decode();

        switch(result) {
            case H264bsdDecoder.ERROR:
                throw new Error('Decoder Error');
            case H264bsdDecoder.PARAM_SET_ERROR:
                throw new Error('Parameter Set Error');
            case H264bsdDecoder.MEMALLOC_ERROR:
                throw new Error('MemAlloc Error');
        }
    }

    var end = process.hrtime();
    var totalSeconds = (end[0] + end[1] / 1e9) - (start[0] + start[1] / 1e9);
    var fps = pictureCount / totalSeconds
    console.log("Decoded %d frames in %s seconds (%s fps)", pictureCount, totalSeconds.toFixed(2), fps.toFixed(2));
}

main();
