import TinyFieldbusController from "../src.js/TinyFieldbusController.js";
import TinyFieldbusDevice from "../src.js/TinyFieldbusDevice.js";
import {ResolvablePromise, arrayBufferToString, EventCapture, concatUint8Arrays} from "../src.js/js-util.js";
import Bus, {encodeFrame, decodeFrameData} from "./Bus.js";
import TFB from "../src.js/tfb.js";

describe("tiny fieldbus",()=>{
    beforeEach(function() {
		TFB.tfb_srand(0);
		jasmine.clock().install();
		jasmine.clock().mockDate(new Date(1000));
    });

    afterEach(function() {
		jasmine.clock().uninstall();
    });

	it("sends controller session announcements",async ()=>{
		let bus=new Bus();
		let c=new TinyFieldbusController({port: bus.createPort()});
		jasmine.clock().tick(1000);
		expect(bus.frame_log.length).toEqual(1);

		jasmine.clock().tick(10000);
		expect(bus.frame_log.length).toBeGreaterThan(5);

		//console.log(bus.frame_log);
	});

	it("sends ack before delivering to app",async ()=>{
		let bus=new Bus();
		let dev=new TinyFieldbusDevice({port: bus.createPort(), name: "hello", type: "world"});
		let events=[];

		bus.on("frame",frame=>events.push(frame));
		dev.addEventListener("message",ev=>events.push(ev));

		bus.writeFrame({assign_name: 'hello', to: 1, session_id: 1234});
		bus.writeFrame({to: 1, seq: 1, payload: "the message"});
		jasmine.clock().tick(1000);

		//console.log(bus.frame_log);
		//console.log(events);

		expect(events).toContain({ to: 1, seq: 1, payload: 'the message', checksum: 37 });
		expect(events).toContain({ from: 1, ack: 1, checksum: 41 });
		expect(events[events.length-1]).type=="message";
	});

	it("can check connection and closes itself on missed ack",async ()=>{
		let event_log=[];
		let bus=new Bus();
		let c=new TinyFieldbusController({port: bus.createPort()});
		let d=new TinyFieldbusDevice({port: bus.createPort(), name: "hello", type: "world"});
		d.addEventListener("connect",()=>event_log.push("connect"));
		d.addEventListener("close",()=>event_log.push("close"));
		expect(d.isConnected()).toEqual(false);

		jasmine.clock().tick(1000);
		expect(event_log).toEqual(["connect"]);
		expect(d.isConnected()).toEqual(true);
		//console.log(bus.frame_log);
		expect(bus.frame_log).toEqual([
			{ announce_name: 'hello', announce_type: 'world', checksum: 113 },
			{ assign_name: 'hello', to: 1, session_id: 27579, checksum: -71 },
			{ session_id: 27579, checksum: -117 },
			{ from: 1, checksum: 25 }
		]);

		bus.removePort(c.port);

		d.send("hello");
		jasmine.clock().tick(10000);
		expect(event_log).toEqual(["connect","close"]);

		expect(d.isConnected()).toEqual(false);
	});

	it("closes itself if controller not seen",async ()=>{
		let bus=new Bus();
		let c=new TinyFieldbusController({port: bus.createPort()});
		let d=new TinyFieldbusDevice({port: bus.createPort(), name: "hello", type: "world"});
		expect(d.isConnected()).toEqual(false);

		jasmine.clock().tick(10000);
		expect(d.isConnected()).toEqual(true);

		bus.removePort(c.port);
		jasmine.clock().tick(10000);
		expect(d.isConnected()).toEqual(false);
	});

	it("sends device announcements on session announcements",async ()=>{
		let bus=new Bus();
		let device=new TinyFieldbusDevice({port: bus.createPort(), name: "hello", type: "world"});
		jasmine.clock().tick(1000);
		let controller=new TinyFieldbusController({port: bus.createPort()});
		jasmine.clock().tick(5000);
		expect(device.isConnected()).toEqual(true);

		expect(bus.frame_log.slice(0,8)).toEqual([
			{ announce_name: 'hello', announce_type: 'world', checksum: 113 },
			{ session_id: 21335, checksum: 95 },
			{ announce_name: 'hello', announce_type: 'world', checksum: 113 },
			{ assign_name: 'hello', to: 1, session_id: 21335, checksum: 109 },
			{ session_id: 21335, checksum: 95 },
			{ from: 1, checksum: 25 },
			{ session_id: 21335, checksum: 95 },
			{ from: 1, checksum: 25 }
		]);

		//console.log(bus.frame_log.slice(0,8));
	});

	it("removes devices on missing ack",async ()=>{
		let bus=new Bus();
		let device=new TinyFieldbusDevice({port: bus.createPort(), name: "hello", type: "world"});
		let controller=new TinyFieldbusController({port: bus.createPort()});
		jasmine.clock().tick(1000);

		bus.removePort(device.port);

		let deviceEp=controller.getDeviceByName("hello");
		let deviceEpEvents=new EventCapture(deviceEp,["close"]);
		deviceEp.send("there?");
		jasmine.clock().tick(10000);

		//console.log(bus.frame_log);
		expect(deviceEpEvents.events.length).toEqual(1);
		expect(deviceEpEvents.events[0].type).toEqual("close");
		expect(Object.values(controller.devicesByName).length).toEqual(0);
	});

	it("removes devices on no activity",async ()=>{
		let bus=new Bus();
		let device=new TinyFieldbusDevice({port: bus.createPort(), name: "hello", type: "world"});
		let controller=new TinyFieldbusController({port: bus.createPort()});
		jasmine.clock().tick(1000);

		let deviceEp=controller.getDeviceByName("hello");
		//deviceEp.addEventListener("close",()=>console.log("******* closed"));
		let deviceEpEvents=new EventCapture(deviceEp,["close"]);
		jasmine.clock().tick(10000);

		expect(Object.values(controller.devicesByName).length).toEqual(1);

		bus.removePort(device.port);
		jasmine.clock().tick(10000);
		expect(deviceEpEvents.events.length).toEqual(1);
		expect(deviceEpEvents.events[0].type).toEqual("close");
		expect(Object.values(controller.devicesByName).length).toEqual(0);

		//console.log(bus.frame_log);
	});

	//fix this test...
	it("can process 2 messages in one chunk",async ()=>{
		let bus=new Bus();
		let device=new TinyFieldbusDevice({port: bus.createPort(), name: "devname", type: "devtype"});
		jasmine.clock().tick(1000);

		bus.writeFrame({assign_name: "devname", to: 123, session_id: 1234});
		/*bus.writeFrame({to: 123, payload: "hello", seq: 1});
		bus.writeFrame({to: 123, payload: "again", seq: 2});*/
		bus.write(concatUint8Arrays(
			encodeFrame({to: 123, payload: "hello", seq: 1}),
			encodeFrame({to: 123, payload: "again", seq: 2})
		));

		jasmine.clock().tick(1000);

		expect(TFB.tfb_get_session_id(device.tfb)).toEqual(1234);
		expect(bus.frame_log).toContain({ from: 123, ack: 1, checksum: 83 });
		expect(bus.frame_log).toContain({ from: 123, ack: 2, checksum: 80 });
		//console.log(bus.frame_log);
	});

	it("assigns",async ()=>{
		let bus=new Bus();
		let controller=new TinyFieldbusController({port: bus.createPort()});
		let controllerEvents=new EventCapture(controller,["device"]);
		let device=new TinyFieldbusDevice({port: bus.createPort(), name: "devname", type: "devtype"});

		jasmine.clock().tick(1000);

		expect(controllerEvents.events.length).toEqual(1);
		let deviceEp=controllerEvents.events[0].device;
		expect(deviceEp.name).toEqual("devname");
	});

	it("can send and receive",async ()=>{
		let bus=new Bus();
		let controller=new TinyFieldbusController({port: bus.createPort()});
		let controllerEvents=new EventCapture(controller,["device"]);
		let device=new TinyFieldbusDevice({port: bus.createPort(), name: "devname", type: "devtype"});
		let deviceEvents=new EventCapture(device,["message"]);

		jasmine.clock().tick(1000);
		let deviceEp=controllerEvents.events[0].device;
		let deviceEpEvents=new EventCapture(deviceEp,["message"]);

		deviceEp.send("hello from the controller");
		deviceEp.send("hello again from the controller");
		device.send("hello");
		device.send("world");
		device.send("again");

		jasmine.clock().tick(10000);
		expect(deviceEpEvents.events.length).toEqual(3);
		expect(deviceEvents.events.length).toEqual(2);
		expect(arrayBufferToString(deviceEvents.events[0].data)).toEqual("hello from the controller");
		expect(arrayBufferToString(deviceEpEvents.events[1].data)).toEqual("world");
	});
});