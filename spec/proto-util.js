import {SyncEventTarget, PropEvent} from "../lib/js-util.js";

let frameTypes=[
	{key: "checksum", num: 1,},
	{key: "from", num: 2,},
	{key: "to", num: 3,},
	{key: "payload", num: 4, type: "string"},
	{key: "seq", num: 5,},
	{key: "ack", num: 6,},
	{key: "announce_name", num: 7, type: "string"},
	{key: "assign_name", num: 8, type: "string"},
	{key: "session_id", num: 9,},
	{key: "reset_to", num: 10,},
];

let frameTypesByKey=Object.fromEntries(frameTypes.map(f=>[f.key,f]));
let frameTypesByNum=Object.fromEntries(frameTypes.map(f=>[f.num,f]));

export function splitFrameBytes(bytes) {
	let chunks=bytes.reduce((a,b)=>(b===0x7e?a.push([]):a[a.length-1].push(b),a),[[]]);
	chunks=chunks.filter(i=>i.length);
	chunks=chunks.map(c=>{
		let res=[];
		for (let i=0; i<c.length; i++) {
			if (c[i]==0x7d) {
				i++;
				res.push((c[i]^0x20)&0xff);
				//throw new Error("Escaping not implemented...");
			}

			else res.push(c[i]);
		}

		return res;
	});

	return chunks;
}

export function decodeFrame(bytes) {
	let index=0,frame_object={};

	while (index<bytes.length) {
		let k=(bytes[index]>>3);
		let z=(bytes[index]&7);
		index++;
		let size;
		switch (z) {
			case 1: size=1; break;
			case 2: size=2; break;
			case 3: size=4; break;
			case 5: size=bytes[index++]; break;
			case 6: size=bytes[index++]*256+bytes[index++]; break;
			default:
				console.log("Warning, unknown size: "+z+" bytes="+bytes.length);
				return {};
		}

		let data=bytes.slice(index,index+size);
		index+=size;

		if (frameTypesByNum[k].type=="string") {
			//console.log("data: ",data);
			frame_object[frameTypesByNum[k].key]=data.map(c=>String.fromCharCode(c)).join("");
		}

		else {
			let val;
			switch (size) {
				case 1: val=data[0]; break;
				case 2: val=256*data[0]+data[1]; break;
				default: throw new Error("Unknown num size: "+size);
			}

			frame_object[frameTypesByNum[k].key]=val;
		}

		//console.log(k,data);
	}

	return frame_object;
}

export function decodeFrameBytes(bytes) {
	function computeCheck(frameBytes) {
		let check=0;
		for (let byte of frameBytes)
			check=(check^byte);

		/*if (check)
			console.log("***** bad check: "+check);*/

		return check;
	}

	return splitFrameBytes(bytes).filter(b=>!computeCheck(b)).map(f=>decodeFrame(f));
}

function encodeValue(value) {
	let bytes;
	if (typeof value=="number" && value<128)
		bytes=[value&0xff];

	else if (typeof value=="number")
		bytes=[value>>8,value&0xff];

	else
		bytes=Array.from(value);

	for (let i=0; i<bytes.length; i++)
		if (typeof bytes[i]=="string")
			bytes[i]=bytes[i].charCodeAt(0);

	let res=[];
	switch (bytes.length) {
		case 1:
			res.push(1);
			break;

		case 2:
			res.push(2);
			break;

		case 4:
			res.push(3);
			break;

		default:
			if (bytes.length<128)
				res.push(5,bytes.length);

			else
				res.push(6,bytes.length>>8,bytes.length&0xff);

			break;
	}

	res.push(...bytes);

	return res;
}

export function encodeFrame(frameObject) {
	let bytes=[];

	for (let k in frameObject) {
		let val=encodeValue(frameObject[k]);
		val[0]|=(frameTypesByKey[k].num<<3);
		bytes.push(...val);
	}

	let c=0;
	for (let byte of bytes)
		c^=byte;

	c^=((1<<3)+1);

	//console.log(c);

	bytes.push(9);
	bytes.push(c);

	return bytes;
}

export function encodeFrameBytes(bytes) {
	let res=[];
	res.push(0x7e);

	for (let byte of bytes) {
		if (byte==0x7e || byte==0x7d) {
			res.push(0x7d);
			res.push(byte^0x20);
		}

		else {
			res.push(byte);
		}
	}

	res.push(0x7e);
	return new Uint8Array(res);
}

export class FrameLogger extends SyncEventTarget {
	constructor() {
		super();
		this.buffer=[];
	}

	write(byte) {
		if (byte==0x7e) {
			if (this.buffer.length && this.seenDelimiter) {
				let frames=decodeFrameBytes(this.buffer);
				for (let frame of frames)
					this.dispatchEvent(new PropEvent("frame",{frame}));
			}

			this.buffer=[];
			this.seenDelimiter=true;
		}

		else {
			this.buffer.push(byte);
		}
	}
}