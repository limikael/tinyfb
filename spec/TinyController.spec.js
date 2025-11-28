import TinyController from "../lib/TinyController.js";
import {EventCapture, arrayBufferToString, concatUint8Arrays} from "../lib/js-util.js";
import Bus from "./Bus.js";
import {splitFrameBytes, decodeFrame, decodeFrameBytes, encodeFrame, encodeFrameBytes} from "./proto-util.js";
import TFB from "../lib/tfb.js";

describe("tiny fieldbus controller",()=>{
    beforeEach(function() {
		//TFB._tfb_srand(0);
		jasmine.clock().install();
		jasmine.clock().mockDate(new Date(1000));
    });

    afterEach(function() {
		jasmine.clock().uninstall();
    });

	it("sends controller session announcements",async ()=>{
		//console.log("start session...");
		let bus=new Bus();
		let c=new TinyController({port: bus.createPort()});
		jasmine.clock().tick(500);
		expect(bus.byte_log.length).toEqual(7);
		//console.log(bus.byte_log);
		for (let i=0; i<100; i++) {
			jasmine.clock().tick(100);
			await Promise.resolve();
		}
		//console.log(bus.byte_log);


		expect(bus.byte_log.length).toEqual(77);

		//c.close();
		//console.log("session done...");
	});
});