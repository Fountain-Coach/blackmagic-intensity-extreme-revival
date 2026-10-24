// Browser Scratch acceptance via Chrome DevTools Protocol.
// No npm dependencies; Node 22+ and a locally opened Chrome debugger required.
// Operates on a synthetic source and never invokes HDMI or DeckLink.
import assert from "node:assert/strict";
import {writeFileSync} from "node:fs";
const origin=process.argv[2]||"http://127.0.0.1:9333";
const screenshot=process.argv[3]||null;
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const tabs=await(await fetch(origin+"/json/list")).json();
const tab=tabs.find(x=>x.type==="page"&&x.url.includes("/scratch"));
if(!tab)throw Error("Chrome /scratch page missing");
const ws=new WebSocket(tab.webSocketDebuggerUrl);
await new Promise((resolve,reject)=>{
 ws.addEventListener("open",resolve,{once:true});
 ws.addEventListener("error",reject,{once:true});
});
let id=0;
const outstanding=new Map();
ws.addEventListener("message",event=>{
 const message=JSON.parse(event.data);
 if(!message.id)return;
 const pending=outstanding.get(message.id);
 if(!pending)return;
 outstanding.delete(message.id);
 message.error?pending.reject(Error(JSON.stringify(message.error))):
               pending.resolve(message.result);
});
function rpc(method,params={}){
 return new Promise((resolve,reject)=>{
  const current=++id;
  outstanding.set(current,{resolve,reject});
  ws.send(JSON.stringify({id:current,method,params}));
 });
}
async function evalJS(expression){
 const r=await rpc("Runtime.evaluate",{expression,returnByValue:true,awaitPromise:true});
 if(r.exceptionDetails)throw Error("Browser JS error: "+JSON.stringify(r.exceptionDetails));
 return r.result.value;
}
async function state(){
 return evalJS("(()=>({frame:Number(document.querySelector('#timeline').value),"
   +"oldest:Number(document.querySelector('#timeline').min),"
   +"latest:Number(document.querySelector('#timeline').max),"
   +"count:Number(document.querySelector('#count').textContent),"
   +"mode:document.querySelector('#transport').textContent,"
   +"status:document.querySelector('#status').textContent,"
   +"provenance:document.querySelector('#provenance').textContent}))()");
}
const ok=name=>console.log("stage=browser_scratch_check result=pass case="+name);
try{
 await rpc("Runtime.enable");await rpc("Page.enable");await sleep(1200);
 let s=await state();
 assert(s.count>=12,"No substantial rolling source history");
 assert(s.provenance.includes("SYNTHETIC"),"Source origin label absent");
 assert(s.status==="Active","Synthetic sender is not active");ok("live_ingest");

 await evalJS("document.querySelector('#freeze').click()");
 let frozen=await state();assert(frozen.mode.includes("FROZEN"));
 await sleep(600);
 assert.equal((await state()).frame,frozen.frame,"freeze did not hold playhead");
 ok("freeze_during_ingest");

 await evalJS("document.querySelector('#back').click()");
 const stepped=await state();
 assert.equal(stepped.frame,frozen.frame-1,"one-frame backward step failed");
 ok("frame_step");

 await evalJS("document.querySelector('#reverse').click()");
 await sleep(700);let reversed=await state();
 assert(reversed.frame<stepped.frame,"reverse shuttle did not move");
 assert(reversed.mode.includes("REVERSE"));ok("reverse_shuttle");

 await evalJS("document.querySelector('#play').click()");
 await sleep(600);let advanced=await state();
 assert(advanced.frame>reversed.frame,"forward shuttle did not move");
 ok("forward_shuttle");

 await evalJS("document.querySelector('#live').click()");
 await sleep(450);let live=await state();
 assert(live.frame>=live.latest-2,"return to live did not catch up");
 assert(live.mode.includes("LIVE"));ok("return_to_live");

 await evalJS("document.querySelector('#freeze').click()");
 const before=await state();
 const position=await evalJS("(()=>{const r=document.querySelector('#jog').getBoundingClientRect();"
    +"return {x:r.x+r.width*.60,y:r.y+r.height*.5};})()");
 await rpc("Input.dispatchMouseEvent",{type:"mouseMoved",x:position.x,y:position.y});
 await rpc("Input.dispatchMouseEvent",{type:"mousePressed",button:"left",clickCount:1,
                                      x:position.x,y:position.y});
 await rpc("Input.dispatchMouseEvent",{type:"mouseMoved",button:"left",
                                      x:position.x-70,y:position.y});
 await rpc("Input.dispatchMouseEvent",{type:"mouseReleased",button:"left",clickCount:1,
                                      x:position.x-70,y:position.y});
 await sleep(150);
 assert((await state()).frame<before.frame,"pointer drag did not jog backward");
 ok("pointer_jog");

 await rpc("Input.dispatchKeyEvent",{type:"keyDown",key:"l",code:"KeyL",windowsVirtualKeyCode:76});
 await sleep(180);assert((await state()).mode.includes("FORWARD"),"L shuttle failed");
 await rpc("Input.dispatchKeyEvent",{type:"keyDown",key:"k",code:"KeyK",windowsVirtualKeyCode:75});
 await sleep(180);assert((await state()).mode.includes("FROZEN"),"K freeze failed");
 ok("jkl_keyboard");

 if(screenshot){
  const frame=await rpc("Page.captureScreenshot",{format:"png",captureBeyondViewport:false});
  writeFileSync(screenshot,Buffer.from(frame.data,"base64"));
  console.log("stage=browser_scratch_screenshot path="+screenshot);
 }
 console.log("stage=browser_scratch_acceptance result=pass checks=8 hardware_access=0");
}finally{ws.close();}
