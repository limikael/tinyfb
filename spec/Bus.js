import EventEmitter from "node:events";

export default class Bus extends EventEmitter {
	constructor() {
		super();
		this.ports=[];
		this.byte_log=[];
	}

	createPort() {
		let port=new EventEmitter();
		port.write=data=>{
			if (!this.ports.includes(port))
				return;

			setTimeout(()=>{
				if (!this.ports.includes(port))
					return;

				this.byte_log.push(...Array.from(data));
				for (let p of this.ports)
					if (p!=port)
						p.emit("data",data);
			},0);
		}

		this.ports.push(port);

		return port;
	}

	removePort(port) {
		if (!port)
			throw new Error("That is not a port");

		let index=this.ports.indexOf(port);
		if (index>=0)
			this.ports.splice(index,1);
	}
}
