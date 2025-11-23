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

export class PropEvent extends Event {
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

export function arrayRemove(a, v) {
    if (a.indexOf(v)<0)
        return;

    a.splice(a.indexOf(v),1);
}

export class EventCapture {
    constructor(dispatcher, eventTypes) {
        this.events=[];

        for (let eventType of eventTypes)
            dispatcher.addEventListener(eventType,ev=>this.events.push(ev));
    }

    getEventTypes() {
        return this.events.map(ev=>ev.type);
    }
}

export function arrayBufferToString(buffer, encoding = 'utf-8') {
    const decoder=new TextDecoder(encoding);
    const uint8Array=new Uint8Array(buffer);
    return decoder.decode(uint8Array);
}

export function concatUint8Arrays(...arrays) {
    const totalLength = arrays.reduce((sum, arr) => sum + arr.length, 0);
    const result = new Uint8Array(totalLength);
    let offset = 0;
    for (const arr of arrays) {
        result.set(arr, offset);
        offset += arr.length;
    }
    return result;
}