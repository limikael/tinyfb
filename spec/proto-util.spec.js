import {splitFrameBytes, decodeFrame, encodeFrame, encodeFrameBytes, decodeFrameBytes} from "./proto-util.js";

describe("proto-util",()=>{
	/*it("can split frames",()=>{
		let bytes=[
		  126,  82, 51, 121,   9, 17,
		  126, 126, 82,  51, 121,  9,
		   17, 126
		];

		let frameBytes=splitFrameBytes(bytes);
		//console.log(frameBytes);
		let frame_objects=frameBytes.map(b=>decodeFrame(b));
		console.log(frame_objects);

		expect(frame_objects).toEqual([
			{ session_id: 13177, checksum: 17 },
			{ session_id: 13177, checksum: 17 }
		]);
	});*/

	it("can encode a frame",()=>{
		let frameObject={to: 1, ack: 123, announce_name: "hello"};
		let frame=encodeFrame(frameObject);
		//console.log(frame);

		let frameBytes=encodeFrameBytes(frame);
		//console.log(frameBytes);

		let res=decodeFrameBytes(frameBytes);
		//console.log(res);
		expect(res).toEqual([ { to: 1, ack: 123, announce_name: 'hello', checksum: 1 } ]);
	});
});