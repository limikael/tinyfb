import TinyDevice from "../lib/TinyDevice.js";
import TinyController from "../lib/TinyController.js";
import {EventCapture, arrayBufferToString, concatUint8Arrays} from "../lib/js-util.js";
import Bus from "./Bus.js";
import {splitFrameBytes, decodeFrame, decodeFrameBytes, encodeFrame, encodeFrameBytes} from "./proto-util.js";
import TFB from "../lib/tfb.js";

describe("tiny fieldbus device",()=>{
    beforeEach(function() {
		TFB._srand(0);
		jasmine.clock().install();
		jasmine.clock().mockDate(new Date(1000));
    });

    afterEach(function() {
		jasmine.clock().uninstall();
    });

	it("sends announcements",async ()=>{
		let bus=new Bus();
		let d=new TinyDevice({port: bus.createPort(), name: "hello"});
		jasmine.clock().tick(500);
		//console.log(decodeFrameBytes(bus.byte_log));
		expect(decodeFrameBytes(bus.byte_log)).toEqual([
			{announce_name: 'hello', checksum: 83}]
		);
	});

	it("can send and recv",async ()=>{
		let bus=new Bus();
		let controller=new TinyController({port: bus.createPort()});
		let controllerEv=new EventCapture(controller,["connect"]);
		let device=new TinyDevice({port: bus.createPort(), name: "hello"});
		let deviceEv=new EventCapture(device,["connect","data"]);

		jasmine.clock().tick(100);
		jasmine.clock().tick(100);
		jasmine.clock().tick(100);

		expect(controllerEv.getEventTypes()).toEqual(["connect"]);
		let deviceEp=controllerEv.events[0].device;
		let deviceEpEv=new EventCapture(deviceEp,["data"]);

		//console.log(decodeFrameBytes(bus.byte_log));
		expect(decodeFrameBytes(bus.byte_log)).toContain({ assign_name: 'hello', to: 1, session_id: 27579, checksum: 169 });

		device.send("bli");
		device.send("bla");
		deviceEp.send("hello from the controller");
		deviceEp.send("xxxxx from the controller again");
		device.send("blu");
		jasmine.clock().tick(10);
		jasmine.clock().tick(10);
		jasmine.clock().tick(10);
		jasmine.clock().tick(10);
		jasmine.clock().tick(10);
		jasmine.clock().tick(10);
		jasmine.clock().tick(10);

		//console.log(decodeFrameBytes(bus.byte_log));
		//console.log(deviceEpEv.getEventTypes());
		//console.log(deviceEv.getEventTypes());
		expect(deviceEv.getEventTypes()).toEqual(["connect","data","data"]);
		expect(deviceEpEv.getEventTypes()).toEqual(["data","data","data"]);
	});

	it("can resend",async ()=>{
		let bus=new Bus();
		let port=bus.createPort();
		let device=new TinyDevice({port: bus.createPort(), name: "hello"});
		let deviceEv=new EventCapture(device,["connect","data","close"]);

		expect(()=>{
			device.send("hello");
		}).toThrow();

		port.write(encodeFrameBytes(encodeFrame({
			assign_name: "hello",
			to: 123
		})));

		jasmine.clock().tick(100);
		jasmine.clock().tick(100);
		jasmine.clock().tick(100);

		device.send("hello");
		jasmine.clock().tick(10);
		device.send("hello2");
		device.send("hello3");
		device.send("hello4");
		jasmine.clock().tick(10);

		jasmine.clock().tick(1000);
		jasmine.clock().tick(1000);
		jasmine.clock().tick(1000);
		jasmine.clock().tick(1000);
		jasmine.clock().tick(1000);

		expect(deviceEv.getEventTypes()).toEqual(["connect","close"]);
		//console.log(deviceEv.getEventTypes());
		//console.log(decodeFrameBytes(bus.byte_log));
	});

	it("can resend when controlled",async ()=>{
		let bus=new Bus();
		let controller=new TinyController({port: bus.createPort()});
		let controllerEv=new EventCapture(controller,["connect"]);
		let device=new TinyDevice({port: bus.createPort(), name: "hello"});
		//let deviceEv=new EventCapture(device,["connect","data"]);

		jasmine.clock().tick(100);
		jasmine.clock().tick(100);
		jasmine.clock().tick(100);

		let deviceEp=controllerEv.events[0].device;
		let deviceEpEv=new EventCapture(deviceEp,["connect","data","close"]);

		bus.removePort(device.physicalPort.port);
		deviceEp.send("hello");

		jasmine.clock().tick(1000);
		jasmine.clock().tick(1000);
		jasmine.clock().tick(1000);
		jasmine.clock().tick(1000);
		jasmine.clock().tick(1000);

		expect(deviceEpEv.getEventTypes()).toEqual(["close"]);
		//console.log(deviceEpEv.getEventTypes());
		//console.log(decodeFrameBytes(bus.byte_log));
	});

	it("sends anouncement as pong reply to session, and handle session change",async ()=>{
		let bus=new Bus();
		let device=new TinyDevice({port: bus.createPort(), name: "hello"});
		let deviceEv=new EventCapture(device,["connect","close","data"]);
		jasmine.clock().tick(100);
		let controller=new TinyController({port: bus.createPort()});

		for (let i=0; i<10; i++)
			jasmine.clock().tick(1000);

		bus.removePort(controller.physicalPort.port);
		controller=new TinyController({port: bus.createPort()});

		for (let i=0; i<10; i++)
			jasmine.clock().tick(1000);

		//console.log(decodeFrameBytes(bus.byte_log));
		expect(deviceEv.getEventTypes()).toEqual(["connect","close","connect"]);
	});

	it("closes if controller not seen",async ()=>{
		let bus=new Bus();
		let device=new TinyDevice({port: bus.createPort(), name: "hello"});
		let deviceEv=new EventCapture(device,["connect","close","data"]);
		let controller=new TinyController({port: bus.createPort()});

		for (let i=0; i<10; i++)
			jasmine.clock().tick(1000);

		expect(deviceEv.getEventTypes()).toEqual(["connect"]);
		bus.removePort(controller.physicalPort.port);

		for (let i=0; i<10; i++)
			jasmine.clock().tick(1000);

		//console.log(decodeFrameBytes(bus.byte_log));
		expect(deviceEv.getEventTypes()).toEqual(["connect","close"]);
	});

	it("closes if device not seen",async ()=>{
		let bus=new Bus();
		let device=new TinyDevice({port: bus.createPort(), name: "hello"});
		let controller=new TinyController({port: bus.createPort()});
		let controllerEv=new EventCapture(controller,["connect"]);

		jasmine.clock().tick(1000);
		let deviceEp=controllerEv.events[0].device;
		let deviceEpEv=new EventCapture(deviceEp,["close"]);

		for (let i=0; i<10; i++)
			jasmine.clock().tick(1000);

		bus.removePort(device.physicalPort.port);

		for (let i=0; i<10; i++)
			jasmine.clock().tick(1000);

		//console.log(decodeFrameBytes(bus.byte_log));
		expect(deviceEpEv.getEventTypes()).toEqual(["close"]);
	});
});