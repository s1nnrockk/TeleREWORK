import http from "node:http";
import net from "node:net";
import { randomBytes, createHmac, timingSafeEqual, createHash } from "node:crypto";
import { URL } from "node:url";

const env = process.env;
const listen = env.LISTEN_ADDR || "127.0.0.1:8080";
const [listenHost, listenPortText] = listen.startsWith("[")
  ? [listen.slice(1, listen.indexOf("]")), listen.slice(listen.indexOf("]") + 2)]
  : listen.split(":");
const port = Number(listenPortText || 8080);
const prefix = (env.PUBLIC_BASE_PATH || "").replace(/^\/+|\/+$/g, "");
if (prefix && !/^[A-Za-z0-9][A-Za-z0-9_-]{0,127}$/.test(prefix)) throw new Error("Invalid PUBLIC_BASE_PATH");
const base = prefix ? `/${prefix}/` : "/";
const endpoint = (p) => `${base}${p}`;
const backendHost = env.MTPROXY_HOST;
const backendPort = Number(env.MTPROXY_PORT || 443);
const bridgeSecret = env.BRIDGE_SECRET;
const sessionTtl = Math.min(3600, Math.max(60, Number(env.SESSION_TTL || 900)));
if (!backendHost || !bridgeSecret || bridgeSecret.length < 32) {
  throw new Error("Set MTPROXY_HOST and a BRIDGE_SECRET of at least 32 characters");
}
const MAX_FRAME = 1024 * 1024;
const WINDOW = 4 * 1024 * 1024;
const MAX_SESSIONS = 256;
const sessions = new Map();

function b64url(buf) { return Buffer.from(buf).toString("base64url"); }
function derive(host, path, secretHex) {
  const context = path
    ? `tdesktop-web-proxy-bridge-v2\n${host}\n${path}`
    : `tdesktop-web-proxy-bridge-v1\n${host}`;
  return createHmac("sha256", Buffer.from(secretHex, "hex")).update(context).digest("base64url");
}
function safeEqual(a, b) {
  const aa = Buffer.from(String(a)); const bb = Buffer.from(String(b));
  return aa.length === bb.length && timingSafeEqual(aa, bb);
}
function parseFrames(buf) {
  const frames = [];
  let offset = 0;
  while (offset < buf.length) {
    if (buf.length - offset < 8) throw new Error("partial frame header");
    const type = buf[offset];
    const stream = buf.readUIntBE(offset + 1, 3);
    const length = buf.readUInt32BE(offset + 4);
    if (length > MAX_FRAME || offset + 8 + length > buf.length) throw new Error("invalid frame length");
    frames.push({ type, stream, payload: buf.subarray(offset + 8, offset + 8 + length) });
    offset += 8 + length;
  }
  if (!frames.length) throw new Error("empty frame message");
  return frames;
}
function frame(type, stream, payload = Buffer.alloc(0)) {
  if (payload.length > MAX_FRAME) throw new Error("frame too large");
  const out = Buffer.allocUnsafe(8 + payload.length);
  out[0] = type; out.writeUIntBE(stream, 1, 3); out.writeUInt32BE(payload.length, 4);
  payload.copy(out, 8); return out;
}
function json(res, status, value) {
  const body = Buffer.from(JSON.stringify(value));
  res.writeHead(status, { "content-type":"application/json; charset=utf-8", "content-length":body.length, "cache-control":"no-store", "x-content-type-options":"nosniff" });
  res.end(body);
}
function readBody(req, limit = 2 * 1024 * 1024) {
  return new Promise((resolve, reject) => {
    const chunks=[]; let size=0;
    req.on("data", c => { size += c.length; if (size > limit) { reject(new Error("body too large")); req.destroy(); } else chunks.push(c); });
    req.on("end", () => resolve(Buffer.concat(chunks)));
    req.on("error", reject);
  });
}
function getSession(req) {
  const token = req.headers.authorization?.replace(/^Bearer\s+/i, "");
  if (!token) return null;
  const hash = createHash("sha256").update(token).digest("hex");
  const s = sessions.get(hash);
  if (!s || s.expires < Date.now()) { if (s) closeSession(hash); return null; }
  return s;
}
function send(s, data) {
  if (s.closed) return;
  if (s.queueBytes + data.length > 32 * 1024 * 1024) { closeSession(s.id); return; }
  s.queue.push(data); s.queueBytes += data.length;
  const waiter = s.downWaiter; s.downWaiter = null;
  if (waiter) flushDown(s, waiter);
}
function flushDown(s, res) {
  if (s.closed) return json(res, 410, { error:"closed" });
  if (!s.queue.length) {
    s.downWaiter = res;
    s.downTimer = setTimeout(() => { if (s.downWaiter === res) { s.downWaiter=null; json(res, 200, { frames:[] }); } }, 20000);
    res.on("close", () => { if (s.downWaiter === res) { clearTimeout(s.downTimer); s.downWaiter=null; } });
    return;
  }
  const batch=[]; let bytes=0;
  while (s.queue.length && bytes + s.queue[0].length <= 2 * 1024 * 1024) {
    const b=s.queue.shift(); batch.push(b); bytes+=b.length; s.queueBytes-=b.length;
  }
  json(res, 200, { frames:batch.map(b=>b.toString("base64")) });
}
function closeSession(id) {
  const s = sessions.get(id); if (!s) return;
  s.closed=true; sessions.delete(id);
  if (s.downWaiter) { clearTimeout(s.downTimer); json(s.downWaiter, 410, {error:"closed"}); s.downWaiter=null; }
  for (const stream of s.streams.values()) stream.socket.destroy();
  s.streams.clear(); s.queue.length=0; s.queueBytes=0;
}
function openStream(s, id) {
  if (id === 0 || s.streams.has(id) || s.streams.size >= 64) throw new Error("invalid OPEN");
  const socket = net.createConnection({host:backendHost,port:backendPort});
  const stream = {socket, recv:WINDOW, send:WINDOW, pending:[], pendingBytes:0};
  s.streams.set(id,stream);
  socket.setNoDelay(true);
  socket.on("connect",()=>send(s,frame(0x04,id,Buffer.alloc(4).writeUInt32BE(0)===0?(()=>{const b=Buffer.alloc(4);b.writeUInt32BE(WINDOW);return b;})():Buffer.alloc(4))));
  socket.on("data",chunk=>{
    if (stream.closed) return;
    let offset=0;
    while(offset<chunk.length) {
      const n=Math.min(64*1024,chunk.length-offset);
      const part=chunk.subarray(offset,offset+n);
      if(part.length>stream.recv){closeSession(s.id);return;}
      stream.recv-=part.length; send(s,frame(0x02,id,part)); offset+=n;
    }
  });
  socket.on("error",()=>{send(s,frame(0x03,id));s.streams.delete(id);});
  socket.on("close",()=>{if(s.streams.has(id)){send(s,frame(0x03,id));s.streams.delete(id);}});
}
function handleFrames(s, frames) {
  for(const f of frames) {
    if(f.stream===0) {
      if(f.type===0x10 && f.payload.length===1 && f.payload[0]===1) send(s,frame(0x11,0));
      else if(f.type===0x05) send(s,frame(0x06,0,f.payload));
      else if(f.type===0x1f) closeSession(s.id);
      else throw new Error("invalid session frame");
      continue;
    }
    const stream=s.streams.get(f.stream);
    if(f.type===0x01) { if(f.payload.length) throw new Error("OPEN payload must be empty"); openStream(s,f.stream); continue; }
    if(!stream) throw new Error("unknown stream");
    if(f.type===0x02) {
      if(f.payload.length>stream.send) throw new Error("send window exceeded");
      stream.send-=f.payload.length;
      if(!stream.socket.destroyed) stream.socket.write(f.payload);
    } else if(f.type===0x04) {
      if(f.payload.length!==4) throw new Error("bad WINDOW");
      const delta=f.payload.readUInt32BE(0);
      if(!delta || stream.send+delta>WINDOW) throw new Error("invalid WINDOW delta");
      stream.send+=delta;
    } else if(f.type===0x03) {
      stream.socket.destroy();s.streams.delete(f.stream);
    } else throw new Error("invalid stream frame");
  }
}
function bridgeHtml(capability) {
  const nonce=b64url(randomBytes(18));
  return `<!doctype html><html><head><meta charset="utf-8"><meta name="referrer" content="no-referrer"></head><body><script nonce="${nonce}">
(async()=>{
 const cap=${JSON.stringify(capability)};
 const origin=location.origin;
 let port=null, token=null, running=false, cursor=0;
 const post=(x)=>{if(port)port.postMessage(x)};
 async function api(path,body){
   const r=await fetch(${JSON.stringify(base)}+path,{method:"POST",headers:{"content-type":"application/json",...(token?{"authorization":"Bearer "+token}:{})},body:JSON.stringify(body||{}),cache:"no-store"});
   if(!r.ok)throw new Error("relay "+r.status);return r.json();
 }
 async function start(p){
   port=p; const s=await api("api/v1/session",{cap});
   token=s.token; post({t:"status",state:"connected"});
   running=true;
   const down=(async()=>{while(running){try{const d=await api("api/v1/down",{});for(const b of d.frames||[])port.postMessage(Uint8Array.from(atob(b),c=>c.charCodeAt(0)).buffer)}catch(e){post({t:"status",state:"reconnecting"});await new Promise(r=>setTimeout(r,500));}}})();
   const up=async(buf)=>{const bytes=new Uint8Array(buf);await api("api/v1/up",{frames:btoa(String.fromCharCode(...bytes))});};
   port.onmessage=async e=>{if(e.data?.t==="close"){running=false;try{await api("api/v1/close",{});}catch{}port.close();return;}if(e.data instanceof ArrayBuffer){try{await up(e.data)}catch{post({t:"status",state:"reconnecting"})}}};
   post({t:"ready"}); await down;
 }
 addEventListener("message",e=>{
   if(e.origin!==origin||e.source!==parent||e.data?.t!=="tproxy-init"||e.data.v!==1||!e.ports?.[0])return;
   start(e.ports[0]).catch(()=>post({t:"status",state:"failed"}));
 },{once:true});
})();
</script></body></html>`;
}
const server=http.createServer(async(req,res)=>{
  const url=new URL(req.url,"http://localhost");
  if(req.method==="GET" && url.pathname===base){
    const cap=url.searchParams.get("bridge");
    if(cap && /^[A-Za-z0-9_-]{43}$/.test(cap)) {
      const page=Buffer.from(bridgeHtml(cap));
      res.writeHead(200,{"content-type":"text/html; charset=utf-8","content-length":page.length,"cache-control":"no-store","content-security-policy":`default-src 'none'; script-src 'nonce-${pageNoncePlaceholder()}' 'unsafe-inline'; connect-src 'self'; frame-ancestors http://127.0.0.1:*; base-uri 'none'; form-action 'none'`,"x-content-type-options":"nosniff"});
      res.end(page);
    } else { res.writeHead(200,{"content-type":"text/html; charset=utf-8","cache-control":"no-store"});res.end("<!doctype html><title>Welcome</title><p>Website</p>"); }
    return;
  }
  if(req.method==="POST" && url.pathname===endpoint("api/v1/session")){
    try{
      const body=JSON.parse((await readBody(req)).toString("utf8"));
      if(typeof body.cap!=="string"||!safeEqual(body.cap,derive(env.PUBLIC_HOST||"proxy.example.com",prefix,env.MTPROXY_SECRET_HEX||""))) return json(res,403,{error:"invalid capability"});
      if(sessions.size>=MAX_SESSIONS)return json(res,503,{error:"capacity"});
      const token=b64url(randomBytes(32));const id=createHash("sha256").update(token).digest("hex");
      sessions.set(id,{id,expires:Date.now()+sessionTtl*1000,streams:new Map(),queue:[],queueBytes:0,downWaiter:null,closed:false});
      return json(res,201,{token,expires_in:sessionTtl});
    }catch{return json(res,400,{error:"bad request"});}
  }
  const s=getSession(req); if(!s)return json(res,401,{error:"unauthorized"});
  if(req.method==="POST"&&url.pathname===endpoint("api/v1/up")){
    try{const body=JSON.parse((await readBody(req,3*1024*1024)).toString("utf8"));const raw=Buffer.from(body.frames||"","base64");handleFrames(s,parseFrames(raw));return json(res,200,{ok:true});}catch(e){closeSession(s.id);return json(res,400,{error:"invalid frames"});}
  }
  if(req.method==="POST"&&url.pathname===endpoint("api/v1/down"))return flushDown(s,res);
  if(req.method==="POST"&&url.pathname===endpoint("api/v1/close")){closeSession(s.id);return json(res,200,{ok:true});}
  json(res,404,{error:"not found"});
});
function pageNoncePlaceholder(){return "invalid";}
server.listen(port,listenHost,()=>console.log(`relay listening on ${listenHost}:${port}; backend ${backendHost}:${backendPort}`));
process.on("SIGTERM",()=>{server.close();for(const id of sessions.keys())closeSession(id);});
