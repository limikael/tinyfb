export function arrayBufferToString(buffer, encoding = 'utf-8') {
	const decoder=new TextDecoder(encoding);
	const uint8Array=new Uint8Array(buffer);
	return decoder.decode(uint8Array);
}

export class ResolvablePromise extends Promise {
    constructor(cb = () => {}) {
        let resolveClosure = null;
        let rejectClosure = null;

        super((resolve,reject)=>{
            resolveClosure = resolve;
            rejectClosure = reject;

            return cb(resolve, reject);
        });

        this.resolveClosure = resolveClosure;
        this.rejectClosure = rejectClosure;
    }

    resolve=(result)=>{
        this.resolveClosure(result);
    }

    reject=(reason)=>{
        this.rejectClosure(reason);
    }
}

export class SyncEventTarget {
    constructor() {
        this._listeners = new Map();
    }

    addEventListener(type, listener) {
        if (!this._listeners.has(type)) this._listeners.set(type, []);
        this._listeners.get(type).push(listener);
    }

    removeEventListener(type, listener) {
        const arr = this._listeners.get(type);
        if (!arr) return;
        const idx = arr.indexOf(listener);
        if (idx >= 0) arr.splice(idx, 1);
    }

    dispatchEvent(event) {
        const arr = this._listeners.get(event.type);
        if (!arr) return true;

        // call listeners synchronously
        for (const listener of arr.slice()) { // slice in case listener array changes
            listener.call(this, event);
        }

        return !event.defaultPrevented;
    }
}

export class CustomEvent extends Event {
    constructor(type, props={}) {
        super(type);
        Object.assign(this,props);
    }
}

export function logAndThrow(fn) {
    try {
        return fn();
    }

    catch (e) {
       console.error(e);
        throw e;
    }
}