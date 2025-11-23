import ctor from "../dist/tfb.js";

let TFB=await ctor();

TFB.strdup=s=>{
	if (s===undefined)
		return 0;

	let mem=TFB._malloc(s.length+1);
	for (let i=0; i<s.length; i++)
		TFB.HEAPU8[mem+i]=s[i].charCodeAt(0);

	TFB.HEAPU8[mem+s.length]=0;

	return mem;
}

TFB.getstr=p=>{
	//console.log("get str at: "+p);

	if (!p)
		return;

	let s="";
	while (TFB.HEAPU8[p]) {
		s+=String.fromCharCode(TFB.HEAPU8[p]);
		p++;
	}

	return s;
}

export default TFB;
