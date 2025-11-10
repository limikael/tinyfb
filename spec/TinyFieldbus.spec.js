import TinyFieldbusController from "../src.js/TinyFieldbusController.js";
import TinyFieldbusDevice from "../src.js/TinyFieldbusDevice.js";
import {ResolvablePromise, arrayBufferToString} from "../src.js/js-util.js";
import Bus from "./Bus.js";

describe("tiny fieldbus",()=>{
	describe("",()=>{
	    beforeEach(function() {
			jasmine.clock().install();
	    });

	    afterEach(function() {
			jasmine.clock().uninstall();
	    });

		it("sends controller session announcements",async ()=>{
			jasmine.clock().mockDate(new Date(1000));
			//let promise=new ResolvablePromise();
			let frame_log=[];

			let bus=new Bus();
			bus.on("frame",o=>{
				//console.log(o);
				frame_log.push(o);
			});

			let c=new TinyFieldbusController({port: bus.createPort()});
			jasmine.clock().tick(100);
			expect(frame_log.length).toEqual(1);

			jasmine.clock().tick(10000);
			expect(frame_log.length).toBeGreaterThan(5);

			expect(frame_log[0].hasOwnProperty("session_id")).toBeTrue();
		});
	});

	it("assigns",async ()=>{
		let promise=new ResolvablePromise();
		let frame_log=[];
		let bus=new Bus();
		bus.on("frame",o=>{
			//console.log(o); 
			frame_log.push(o);
			if (o.assign_name)
				promise.resolve();
		});

		let controllerDevice;
		let controller=new TinyFieldbusController({port: bus.createPort()});
		controller.addEventListener("device",ev=>{
			controllerDevice=ev.device;
		});
		let device=new TinyFieldbusDevice({port: bus.createPort(), name: "devname", type: "devtype"});

		await promise;

		//console.log(controllerDevice);
		expect(controllerDevice.name).toEqual("devname");
		expect(controllerDevice.id).toEqual(1);
	});

	it("can send and receive",async ()=>{
		let done=new ResolvablePromise();
		let deviceEndpointPromise=new ResolvablePromise();
		let frame_log=[];
		let messages=[];
		let bus=new Bus();
		bus.on("frame",o=>{
			//console.log(o); 
			frame_log.push(o);
		});

		let controller=new TinyFieldbusController({port: bus.createPort()});
		controller.addEventListener("device",ev=>deviceEndpointPromise.resolve(ev.device));
		let device=new TinyFieldbusDevice({port: bus.createPort(), name: "devname", type: "devtype"});

		let deviceEndpoint=await deviceEndpointPromise;
		deviceEndpoint.addEventListener("message",ev=>{
			messages.push(arrayBufferToString(ev.data));
			if (messages.length==2) {
				expect(messages).toEqual(["hello","again"]);
				done.resolve();
			}
		});

		device.send("hello");
		device.send("again");

		await done;
	});
});