import TFB from "./tfb.js";
import {SyncEventTarget, arrayRemove, PropEvent, logAndThrow, resetTimeout} from "./js-util.js";
import PhysicalPort from "./PhysicalPort.js";
import TinyDevice from "./TinyDevice.js";

export default class TinyController extends SyncEventTarget {
	constructor({port}={}) {
		super();
		this.devices=[];
		this.physicalPort=new PhysicalPort({port});
		this.physicalPort.addEventListener("data",this.tick);
		this.hub=TFB._tfb_hub_create(this.physicalPort.physical);
		TFB._tfb_hub_connect_func(this.hub,TFB.addFunction((_,sock)=>{
			return logAndThrow(()=>{
				let device=new TinyDevice({sock});
				this.devices.push(device);
				this.dispatchEvent(new PropEvent("connect",{device}));
			});
		},"vii"));

		this.tick();
	}

	tick=()=>{
		TFB._tfb_hub_tick(this.hub);
		let t=TFB._tfb_hub_get_timeout(this.hub);
		this.timeoutId=resetTimeout(this.timeoutId,this.tick,t);
	}
}