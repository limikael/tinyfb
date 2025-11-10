import TFB from "./tfb.js";
import TfbHandler from "./TfbHandler.js";
import {CustomEvent, arrayBufferToString} from "./js-util.js";

export default class TinyFieldbusDevice extends EventTarget {
	constructor({port, name, type}={}) {
		super();
		this.tfb=TFB.tfb_create_device(TFB.module.allocateUTF8(name),TFB.module.allocateUTF8(type));
		this.tfbHandler=new TfbHandler({tfb: this.tfb, port});
		this.tfbHandler.addEventListener("message",this.handleMessage);
	}

	send(data) {
		this.tfbHandler.send(data);
	}

	handleMessage=ev=>{
		//console.log("message in dev: "+arrayBufferToString(ev.data));
		this.dispatchEvent(new CustomEvent("message",{
			data: ev.data
		}));
	}
}
