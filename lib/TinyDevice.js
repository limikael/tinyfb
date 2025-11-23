import TFB from "./tfb.js";
import {SyncEventTarget, arrayRemove, PropEvent, logAndThrow} from "./js-util.js";
import PhysicalPort from "./PhysicalPort.js";

export default class TinyDevice extends SyncEventTarget {
	constructor({device, port, name}={}) {
		super();

		if (device) {
			this.device=device;
			this.controlled=true;
		}

		else {
			if (!name)
				throw new Error("Missing name");

			this.physicalPort=new PhysicalPort({port});
			this.physicalPort.addEventListener("data",this.tick);
			this.device=TFB._tfb_device_create(this.physicalPort.physical,TFB.strdup(name));

		}

		TFB._tfb_device_data_func(this.device,TFB.addFunction((_)=>{
			return logAndThrow(()=>{
				let available=TFB._tfb_device_available(this.device);
				let pointer=TFB._malloc(available);
				let res=TFB._tfb_device_recv(this.device,pointer,available);
				let data=TFB.HEAPU8.slice(pointer,pointer+available);
				TFB._free(pointer);
				this.dispatchEvent(new PropEvent("data",{data}));
			});
		},"vi"));

		TFB._tfb_device_status_func(this.device,TFB.addFunction((_)=>{
			logAndThrow(()=>{
				//console.log("status, con="+TFB._tfb_device_is_connected(this.device));

				if (TFB._tfb_device_is_connected(this.device)) {
					//console.log("dispatch connect")
					this.dispatchEvent(new PropEvent("connect"));
				}

				else
					this.dispatchEvent(new PropEvent("close"));
			});
		},"vi"));

		this.tick();
	}

	tick=()=>{
		if (this.controlled)
			return;

		TFB._tfb_device_tick(this.device);

		if (this.timeoutId) {
			clearTimeout(this.timeoutId);
			this.timeoutId=null;
		}

		let t=TFB._tfb_device_get_timeout(this.device);
		if (t>=0)
			this.timeoutId=setTimeout(this.tick,t);
	}

	send(data) {
		if (typeof data=="string")
			data=new TextEncoder().encode(data);

		if (!(data instanceof Uint8Array))
			throw new Error("Need Uint8Array to send");

		let pointer=TFB._malloc(data.length);
		TFB.HEAPU8.set(data,pointer);
		let res=TFB._tfb_device_send(this.device,pointer,data.length);
		TFB._free(pointer);
		this.tick();

		if (res<0)
			throw new Error("Bus Error: "+TFB._tfb_get_errno(this.tfb));
	}
}