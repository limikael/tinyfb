import TFB from "./tfb.js";
import {logAndThrow, SyncEventTarget} from "./js-util.js";

export default class PhysicalPort extends SyncEventTarget {
	constructor({port}) {
		super();

		this.port=port;
		this.physical=TFB._tfb_physical_create();
		this.inbuf=[];

		this.port.on("data",bytes=>{
			for (let byte of Array.from(bytes)) {
				//console.log("got data: "+byte);
				this.inbuf.push(byte);
			}

			this.dispatchEvent(new Event("data"));
		});

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

	write(byte) {
		this.port.write(new Uint8Array([byte]));
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