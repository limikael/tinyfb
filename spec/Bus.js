import EventEmitter from "node:events";
import TFB from "../src.js/tfb.js";
import {logAndThrow} from "../src.js/js-util.js";

let frameTypes=[
	{name: "checksum", value: 1,},
	{name: "from", value: 2,},
	{name: "to", value: 3,},
	{name: "payload", value: 4, type: "string"},
	{name: "seq", value: 5,},
	{name: "ack", value: 6,},
	{name: "announce_name", value: 7, type: "string"},
	{name: "announce_type", value: 8, type: "string"},
	{name: "assign_name", value: 9, type: "string"},
	{name: "session_id", value: 10,},
	{name: "reset_to", value: 11,},
];

let frameTypesByName=Object.fromEntries(frameTypes.map(f=>[f.name,f]));
let frameTypesByValue=Object.fromEntries(frameTypes.map(f=>[f.value,f]));

export default class Bus extends EventEmitter {
	constructor() {
		super();
		this.ports=[];
		this.link=TFB.tfb_link_create();
		this.frame_log=[];

		TFB.tfb_link_frame_func(this.link,TFB.module.addFunction((link, data, size)=>{
			logAndThrow(()=>{
				let frame_object=decodeFrame(TFB.module.HEAPU8.slice(data,data+size));
				this.frame_log.push(frame_object);
			});
		},"viii"));

		let p=this.createPort();
		p.on("data",data=>{
			for (let byte of data)
				TFB.tfb_link_rx_push_byte(this.link,byte);
		});
	}

	writeFrame(frame_object) {
		this.write(createBusFrameData(encodeFrame(frame_object)));
	}

	write(data) {
		for (let p of this.ports)
			p.emit("data",data);
	}

	createPort() {
		let port=new EventEmitter();
		port.write=data=>{
			if (!this.ports.includes(port))
				return;

			for (let p of this.ports)
				if (p!=port)
					p.emit("data",data);
		}

		this.ports.push(port);

		return port;
	}

	removePort(port) {
		let index=this.ports.indexOf(port);
		if (index>=0)
			this.ports.splice(index,1);
	}
}

export function encodeFrame(frame_object) {
	let frame=TFB.tfb_frame_create(1024);
	let bytes=[];

	for (let k in frame_object) {
		let frameType=frameTypesByName[k];
		if (!frameType)
			throw new Error("Unknown frame type: "+k);

		switch (frameType.type) {
			case "string":
				TFB.tfb_frame_write_data(frame,frameType.value,TFB.module.allocateUTF8(frame_object[k]),frame_object[k].length)
				break;

			default:
				TFB.tfb_frame_write_num(frame,frameType.value,frame_object[k])
				break;
		}
	}

	TFB.tfb_frame_write_checksum(frame);
	bytes=[];

	for (let i=0; i<TFB.tfb_frame_get_size(frame); i++)
		bytes.push(TFB.tfb_frame_get_buffer_at(frame,i));

	TFB.tfb_frame_dispose(frame);

	let frame_data=new Uint8Array(bytes);
	return frame_data;
}

export function decodeFrame(frameData) {
	let frame=TFB.tfb_frame_create(1024);
	for (let byte of frameData)
		TFB.tfb_frame_write_byte(frame,byte);

	let frame_object={};
	for (let i=0; i<TFB.tfb_frame_get_num_keys(frame); i++) {
		let key=TFB.tfb_frame_get_key_at(frame,i);
		let frameType=frameTypesByValue[key];
		if (!frameType)
			throw new Error("Unknown key in data: "+key);

		switch (frameType.type) {
			case "string":
				let data=TFB.tfb_frame_get_data(frame,key);
				let size=TFB.tfb_frame_get_data_size(frame,key);
				let s=[...TFB.module.HEAPU8.subarray(data,data+size)].map(c=>String.fromCharCode(c)).join("");
				frame_object[frameType.name]=s;
				break;

			default:
				frame_object[frameType.name]=TFB.tfb_frame_get_num(frame,key);
				break;
		}
	}

	TFB.tfb_frame_dispose(frame);

	return frame_object;
}

export function createBusFrameData(data) {
	let busBytes=[];

	busBytes.push(0x7e);
	for (let byte of data) {
		if (byte==0x7e || byte==0x7d) {
			busBytes.push(0x7d);
			busBytes.push(byte^0x20);
		}

		else {
			busBytes.push(byte);
		}
	}
	busBytes.push(0x7e);

	return new Uint8Array(busBytes);
}