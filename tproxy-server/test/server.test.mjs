import test from "node:test";
import assert from "node:assert/strict";
import { createHmac } from "node:crypto";

function capability(host,path,secret){
 const context=path?`tdesktop-web-proxy-bridge-v2\n${host}\n${path}`:`tdesktop-web-proxy-bridge-v1\n${host}`;
 return createHmac("sha256",Buffer.from(secret,"hex")).update(context).digest("base64url");
}
test("all published bridge capability vectors",()=>{
 const vectors=[
 ["proxy.example.com","","000102030405060708090a0b0c0d0e0f","MHLEY5PmW1GWqJkSrlmJpvJUiLhBH_QKy6yKg8a0JPk"],
 ["proxy.example.com","","dd000102030405060708090a0b0c0d0e0f","IpJrt3e7sKtzPyoXy6w-Zj6GGEvsvclN66JzQEfPYLA"],
 ["proxy.example.com","dobry-cola-super-app","000102030405060708090a0b0c0d0e0f","hHz99Xs93EN1j91G9gpNepXwGNNt5YdAFkEVk_LlqdQ"],
 ["proxy.example.com","dobry-cola-super-app","dd000102030405060708090a0b0c0d0e0f","TGUkZaevsavLbHvlNWipnRoYxgzZ51ioWvbxgGT3wHo"]];
 for(const [h,p,s,e] of vectors)assert.equal(capability(h,p,s),e);
});
