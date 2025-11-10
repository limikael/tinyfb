import TFB from "./tfb.js";
import TfbHandler from "./TfbHandler.js";
import {arrayBufferToString, SyncEventTarget} from "./js-util.js";

export default class TinyFieldbusController extends SyncEventTarget {
	constructor({port}={}) {
		super();
		this.tfb=TFB.tfb_create_controller();
		this.tfbHandler=new TfbHandler({tfb: this.tfb, port});
		this.tfbHandler.addEventListener("device",this.handleDevice);
		this.tfbHandler.addEventListener("message",this.handleMessage);
		this.devicesByName={};
	}

	handleMessage=ev=>{
		if (!ev.from)
			return;

		for (let device of Object.values(this.devicesByName)) {
			if (device.id==ev.from) {
				let messageEv=new Event("message");
				messageEv.data=ev.data;
				device.dispatchEvent(messageEv);
			}
		}
	}

	handleDevice=ev=>{
		if (ev.id) {
			if (this.devicesByName[ev.name]) {
				this.devicesByName[ev.name].id=ev.id;
				return;
			}

			let device=new SyncEventTarget();
			device.send=data=>{
				this.tfbHandler.send_to(data,device.id);
			}

			device.name=ev.name;
			device.id=ev.id;
			this.devicesByName[device.name]=device;

			let deviceEv=new Event("device");
			deviceEv.device=device;
			this.dispatchEvent(deviceEv);
		}

		else {
			if (this.devicesByName[ev.name])
				this.devicesByName[ev.name].dispatchEvent(new Event("close"));

			delete this.devicesByName[ev.name];
		}
	}
}