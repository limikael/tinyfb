import TFB from "./tfb.js";
import {logAndThrow, SyncEventTarget, PropEvent} from "./js-util.js";
import {FrameLogger} from "../spec/proto-util.js";

export default class PhysicalPort extends SyncEventTarget {
	constructor({port}) {
		super();

		this.port=port;
		this.port.on("open",()=>{
			this.port.flush(err=>{
				if (err) {
					console.log("flush error: "+err);
					return;
				}

				let chunk; 
				while ((chunk = port.read()) !== null) { 
					// discarding incoming bytes 
				}

				this.port.on("data",bytes=>{
					//console.log("got data: ",bytes);

					for (let byte of Array.from(bytes)) {
						this.inbuf.push(byte);
						this.inFrameLogger.write(byte);
					}

					this.dispatchEvent(new Event("data"));
				});
			});
		});

		this.physical=TFB._tfb_physical_create();
		this.inbuf=[];
		this.inFrameLogger=new FrameLogger();
		this.inFrameLogger.addEventListener("frame",ev=>{
			this.dispatchEvent(new PropEvent("frame",{frame: ev.frame}));
		})
		this.outFrameLogger=new FrameLogger();
		this.outFrameLogger.addEventListener("frame",ev=>{
			this.dispatchEvent(new PropEvent("frame",{frame: ev.frame}));
		})

		this.busy=false;

		let funcs=[
			[1,"write","iii"],
			[2,"read","ii"],
			[3,"available","ii"],
			[4,"millis","ii"]
		];

		for (let f of funcs) {
			TFB._tfb_physical_func(this.physical,f[0],TFB.addFunction((_,...args)=>{
				//console.log("called from mod: "+f[1]);
				return logAndThrow(()=>{
					return this[f[1]].bind(this)(...args);
				});
			},f[2]));
		}
	}

	isBusy() {
		return this.busy;
	}

	handleDrainComplete=()=>{
		//console.log("drain complete...");

		if (this.drainMore) {
			this.drainMore=false;
			this.port.drain(this.handleDrainComplete);
		}

		else {
			this.busy=false;
			this.dispatchEvent(new Event("busyStateChange"));
		}
	}

	write(byte) {
		this.outFrameLogger.write(byte);
		this.port.write(new Uint8Array([byte]));

		if (this.busy) {
			this.drainMore=true;
		}

		else {
			this.busy=true;
			this.port.drain(this.handleDrainComplete);
			this.dispatchEvent(new Event("busyStateChange"));
		}
	}

	read() {
		if (!this.inbuf.length)
			return -1;

		return this.inbuf.shift();
	}

	available() {
		let av=this.inbuf.length;
		//console.log("returning available: "+av);

		return av;
	}

	millis() {
		return Date.now();
	}

	close() {
		TFB._tfb_physical_dispose(this.physical);
	}
}