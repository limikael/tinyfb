import TFB from "./tfb.js";
import {SyncEventTarget, CustomEvent, logAndThrow} from "./js-util.js";

export default class TfbHandler extends SyncEventTarget {
	constructor({port, tfb}={}) {
		super();

		this.tfb=tfb;
		this.port=port;

		TFB.tfb_message_func(this.tfb,TFB.module.addFunction((data, size)=>{
			logAndThrow(()=>this.dispatchEvent(new CustomEvent("message",{
				data: TFB.module.HEAPU8.buffer.slice(data,data+size),
			})));
		},"vii"));

		TFB.tfb_message_from_func(this.tfb,TFB.module.addFunction((data, size, from)=>{
			logAndThrow(()=>this.dispatchEvent(new CustomEvent("message",{
				data: TFB.module.HEAPU8.buffer.slice(data,data+size),
				from: from
			})));
		},"viii"));

		TFB.tfb_device_func(this.tfb,TFB.module.addFunction((name_ptr)=>{
			logAndThrow(()=>this.dispatchEvent(new CustomEvent("device",{
				name: TFB.module.UTF8ToString(name_ptr),
				id: TFB.tfb_get_device_id_by_name(this.tfb,name_ptr)
			})));
		},"vi"));

		TFB.tfb_status_func(this.tfb,TFB.module.addFunction(()=>{
			logAndThrow(()=>this.dispatchEvent(new CustomEvent("status")));
		},"v"));

		//TFB.tfb_set_id(this.tfb,this.id);
		this.port.on("data",this.handleData);
		this.updateTimeout();
	}

	send(data) {
		if (typeof data=="string")
			data=new TextEncoder().encode(data);

		if (!(data instanceof Uint8Array))
			throw new Error("Need Uint8Array to send");

		let pointer=TFB.module._malloc(data.length);
		TFB.module.HEAPU8.set(data,pointer);
		let res=TFB.tfb_send(this.tfb,pointer,data.length);
		TFB.module._free(pointer);
		this.updateTimeout();

		if (!res)
			throw new Error("Bus Error: "+TFB.tfb_get_errno(this.tfb));
	}

	send_to(data, device_id) {
		if (!device_id)
			throw new Error("No device id for send_to");

		if (typeof data=="string")
			data=new TextEncoder().encode(data);

		if (!(data instanceof Uint8Array))
			throw new Error("Need Uint8Array to send");

		let pointer=TFB.module._malloc(data.length);
		TFB.module.HEAPU8.set(data,pointer);
		let res=TFB.tfb_send_to(this.tfb,pointer,data.length,device_id);
		TFB.module._free(pointer);
		this.updateTimeout();

		if (!res)
			throw new Error("Bus Error: "+TFB.tfb_get_errno(this.tfb));
	}

	/*drain() {
		while (TFB.tfb_tx_is_available(this.tfb)) {
			let byte=TFB.tfb_tx_pop_byte(this.tfb);
			this.port.write(new Uint8Array([byte]))
		}
	}*/

	drain() {
		let bytes=[];
		while (TFB.tfb_tx_is_available(this.tfb))
			bytes.push(TFB.tfb_tx_pop_byte(this.tfb));

		if (bytes.length) {
			this.port.write(new Uint8Array(bytes))
			TFB.tfb_notify_bus_activity(this.tfb);
		}
	}


	handleData=(data)=>{
		for (let byte of data) {
			//console.log("byte: "+byte+" seen by: "+this.id);

			if (!TFB.tfb_rx_is_available(this.tfb)) {
				this.drain();
				TFB.tfb_tick(this.tfb);
				this.drain();
				this.updateTimeout();
			}

			TFB.tfb_rx_push_byte(this.tfb,byte);
		}

		this.drain();
		TFB.tfb_tick(this.tfb);
		this.drain();
		this.updateTimeout();
	}

	handleTimeout=()=>{
		this.drain();
		TFB.tfb_tick(this.tfb);
		this.drain();
		this.updateTimeout();
	}

	updateTimeout() {
		if (this.timeoutId) {
			clearTimeout(this.timeoutId);
			this.timeoutId=null;
		}

		let t=TFB.tfb_get_timeout(this.tfb);
		if (t>=0) {
			//console.log("setting timeout: "+t);
			setTimeout(this.handleTimeout,t);
		}

		else {
			//console.log("no timeout to set");
		}
	}
}