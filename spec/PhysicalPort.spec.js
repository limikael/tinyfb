import PhysicalPort from "../lib/PhysicalPort.js";
import Bus from "./Bus.js";
import {EventCapture} from "../lib/js-util.js";

describe("physical port",()=>{
    beforeEach(function() {
		jasmine.clock().install();
		jasmine.clock().mockDate(new Date(1000));
    });

    afterEach(function() {
		jasmine.clock().uninstall();
    });

	it("works",()=>{
		let bus=new Bus();
		let p1=new PhysicalPort({port: bus.createPort()});
		let p2=new PhysicalPort({port: bus.createPort()});

		p1.write(123);
		p1.write(234);
		jasmine.clock().tick(0);
		expect(p2.read()).toEqual(123);
		expect(p2.read()).toEqual(234);
		expect(p2.read()).toEqual(-1);

		p1.close();
		p2.close();
	});

});

describe("physical port busy",()=>{
	it("has a busy state",async ()=>{
		let bus=new Bus();
		let port=new PhysicalPort({port: bus.createPort()});
		let portEvents=new EventCapture(port,["busyStateChange"]);

		port.addEventListener("busyStateChange",()=>{
			//console.log("busy: "+port.isBusy());
		});

		for (let i=0; i<10; i++)
			port.write(123);

		port.close();

		await new Promise(r=>setTimeout(r,100));
		expect(portEvents.events.map(ev=>ev.type)).toEqual(["busyStateChange","busyStateChange"]);
	});
});
