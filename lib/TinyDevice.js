import TFB from "./tfb.js";
import {SyncEventTarget, arrayRemove, PropEvent, logAndThrow, resetTimeout} from "./js-util.js";
import PhysicalPort from "./PhysicalPort.js";

export default class TinyDevice extends SyncEventTarget {
	constructor({sock, device, port, name}={}) {
		super();

		if (device)
			throw new Error("Renamed!!!");

		if (sock) {
			this.sock=sock;
			this.controlled=true;
		}

		else {
			if (!name)
				throw new Error("Missing name");

			this.physicalPort=new PhysicalPort({port});
			this.physicalPort.addEventListener("data",this.tick);
			this.sock=TFB._tfb_sock_create(this.physicalPort.physical,TFB.strdup(name));
		}

		TFB._tfb_sock_event_func(this.sock,TFB.addFunction((_,ev)=>{
			logAndThrow(()=>this.handleEvent(ev))
		},"vii"));

		this.tick();
	}

	handleEvent=(ev)=>{
		switch (ev) {
			case 1:
				this.dispatchEvent(new PropEvent("connect"));
				break;

			case 2:
				this.dispatchEvent(new PropEvent("close"));
				break;

			case 3:
				let available=TFB._tfb_sock_available(this.sock);
				let pointer=TFB._malloc(available);
				let res=TFB._tfb_sock_recv(this.sock,pointer,available);
				let data=TFB.HEAPU8.slice(pointer,pointer+available);
				TFB._free(pointer);
				this.dispatchEvent(new PropEvent("data",{data: data.buffer}));
				break;

			default:
				throw new Error("Unknown sock event: "+ev);
				break;
		}

		//console.log("socket event: "+ev);
	}

	tick=()=>{
		if (this.controlled)
			return;

		TFB._tfb_sock_tick(this.sock);
		let t=TFB._tfb_sock_get_timeout(this.sock);
		this.timeoutId=resetTimeout(this.timeoutId,this.tick,t);
	}

	send(data) {
		if (typeof data=="string")
			data=new TextEncoder().encode(data);

		if (!(data instanceof Uint8Array))
			throw new Error("Need Uint8Array to send");

		let pointer=TFB._malloc(data.length);
		TFB.HEAPU8.set(data,pointer);
		let res=TFB._tfb_sock_send(this.sock,pointer,data.length);
		TFB._free(pointer);
		this.tick();

		if (res<0)
			console.log("******* bus error!!!");
			//throw new Error("Bus Error: "+TFB._tfb_get_errno(this.tfb));
	}
}