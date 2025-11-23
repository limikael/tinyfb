import PhysicalPort from "../lib/PhysicalPort.js";
import Bus from "./Bus.js";

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