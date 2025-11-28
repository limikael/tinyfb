import TFB from "./tfb.js";
import {SyncEventTarget, arrayRemove, PropEvent, logAndThrow, resetTimeout} from "./js-util.js";
import PhysicalPort from "./PhysicalPort.js";
import TinyDevice from "./TinyDevice.js";
import {SerialPort} from "serialport";

export default class TinyController extends SyncEventTarget {
	constructor({port, logFrames, path, baudRate}={}) {
		super();

		if (!baudRate)
			baudRate=9600;

		if (path) {
			port=new SerialPort({
				path: '/dev/ttyUSB1',
				baudRate: 9600,
			});
		}

		if (!port)
			throw new Error("No port!");

		this.devices=[];
		this.physicalPort=new PhysicalPort({port});
		this.physicalPort.addEventListener("frame",ev=>{
			this.dispatchEvent(new PropEvent("frame",{frame: ev.frame}));
		});

		this.physicalPort.addEventListener("data",this.tick);
		this.physicalPort.addEventListener("busyStateChange",()=>{
			console.log("busy: "+this.physicalPort.isBusy());
			TFB._tfb_hub_set_link_busy(this.hub,this.physicalPort.isBusy());
			setTimeout(this.tick,0);
		});

		this.hub=TFB._tfb_hub_create(this.physicalPort.physical);
		TFB._tfb_hub_event_func(this.hub,TFB.addFunction((_,ev)=>{
			logAndThrow(()=>this.handleEvent(ev))
		},"vii"));

		setTimeout(this.tick,0);

		if (logFrames) {
			this.addEventListener("frame",ev=>{
				console.log("frame: "+JSON.stringify(ev.frame));
			});
		}

		//this.tick();
	}

	handleEvent=(ev)=>{
		switch (ev) {
			case 1:
				let sock=TFB._tfb_hub_accept(this.hub);
				let device=new TinyDevice({sock});
				this.devices.push(device);
				this.dispatchEvent(new PropEvent("connect",{device}));
				break;

			default:
				throw new Error("Unknown hub event: "+ev);
				break;
		}
	}

	tick=()=>{
		TFB._tfb_hub_tick(this.hub);
		let t=TFB._tfb_hub_get_timeout(this.hub);
		this.timeoutId=resetTimeout(this.timeoutId,this.tick,t);
	}
}