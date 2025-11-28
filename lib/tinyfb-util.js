import {SyncEventTarget, PropEvent} from "./js-util.js";

export default class JsonDevice extends SyncEventTarget {
	constructor(tinyDevice) {
		super();
		this.tinyDevice=tinyDevice;
		this.tinyDevice.addEventListener("data",ev=>{
			const text=new TextDecoder().decode(ev.data);
			const obj=JSON.parse(text);
			this.dispatchEvent(new PropEvent("data",{data: obj}));
		});
	}

	send(data) {
		return this.tinyDevice.send(JSON.stringify(data));
	}
}

export function createJsonDevice(tinyDevice) {
	return new JsonDevice(tinyDevice);
}