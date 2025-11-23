import TFB from "./tfb.js";
import {SyncEventTarget, arrayRemove, PropEvent, logAndThrow} from "./js-util.js";
import PhysicalPort from "./PhysicalPort.js";
import TinyDevice from "./TinyDevice.js";

export default class TinyController extends SyncEventTarget {
	constructor({port}={}) {
		super();
		this.devices=[];
		this.physicalPort=new PhysicalPort({port});
		this.physicalPort.addEventListener("data",this.tick);
		this.controller=TFB._tfb_controller_create(this.physicalPort.physical);
		TFB._tfb_controller_device_func(this.controller,TFB.addFunction((_,dev)=>{
			return logAndThrow(()=>{
				let device=new TinyDevice({device: dev});
				this.devices.push(device);
				this.dispatchEvent(new PropEvent("device",{device}));
			});
		},"vii"));

		this.tick();
	}

	tick=()=>{
		TFB._tfb_controller_tick(this.controller);

		if (this.timeoutId) {
			clearTimeout(this.timeoutId);
			this.timeoutId=null;
		}

		let t=TFB._tfb_controller_get_timeout(this.controller);
		if (t>=0)
			this.timeoutId=setTimeout(this.tick,t);
	}
}