const state=document.getElementById("state")
const dot=document.getElementById("dot")

async function update(){

 const res=await fetch("/api/state")
 const data=await res.json()

 state.textContent=data.led?"LED ON":"LED OFF"
 dot.classList.toggle("on",data.led)

}

async function ledOn(){

 await fetch("/api/on")
 update()

}

async function ledOff(){

 await fetch("/api/off")
 update()

}



update()
setInterval(update,2000)

const uptimeEl = document.getElementById("uptime")
const heapEl = document.getElementById("heap")
const rssiEl = document.getElementById("sys_rssi")
const resetEl = document.getElementById("reset")

function fmtUptime(sec){
  const h = Math.floor(sec / 3600)
  const m = Math.floor((sec % 3600) / 60)
  const s = sec % 60
  return `${h}h ${m}m ${s}s`
}

async function updateWeather(){

  try{

    const res = await fetch("/api/weather")
    const data = await res.json()

    if(!data.current_weather) return

    document.getElementById("temp").textContent =
      data.current_weather.temperature + " °C"

    document.getElementById("wind").textContent =
      data.current_weather.windspeed + " km/h"

  }catch(e){
    console.log("weather error",e)
  }

}

updateWeather()

setInterval(updateWeather,600000)

async function updateSystem(){
  const res = await fetch("/api/system", { cache: "no-store" })
  const d = await res.json()

  uptimeEl.textContent = fmtUptime(d.uptime_s)
  heapEl.textContent = `${d.heap} bytes`
  rssiEl.textContent = `${d.rssi} dBm`
  resetEl.textContent = d.reset_reason || "—"
}

async function scanWifi(){

const list=document.getElementById("wifiList")

list.innerHTML="Scanning..."

const res=await fetch("/api/wifi")
const networks=await res.json()

networks.sort((a,b)=>b.rssi-a.rssi)

list.innerHTML=""

networks.forEach(n=>{

const bars=Math.min(4,Math.max(1,Math.floor((n.rssi+100)/12)))

let barHTML=""

for(let i=1;i<=4;i++){
barHTML+=`<div class="bar bar${i} ${i<=bars?"on":""}"></div>`
}

const el=document.createElement("div")

el.className="wifi"

el.innerHTML=`
<div>
<div class="wifi-name">${n.ssid||"(hidden)"}</div>
<div class="wifi-meta">${n.rssi} dBm • ch ${n.channel}</div>
</div>

<div style="display:flex;align-items:center;gap:10px">

<div class="badge ${n.secure?"secure":"open"}">
${n.secure?"secure":"open"}
</div>

<div class="signal">
${barHTML}
</div>

</div>
`

list.appendChild(el)

})

}

const clockEl = document.getElementById("clock")
const dateEl = document.getElementById("date")

function pad2(n){ return String(n).padStart(2,"0") }

async function updateTime(){
  const r = await fetch("/api/time", { cache: "no-store" })
  const t = await r.json()

  clockEl.textContent = `${pad2(t.h)}:${pad2(t.m)}:${pad2(t.sec)}`
  dateEl.textContent = (t.local || "—").slice(0,10)
}

updateTime()
setInterval(updateTime, 1000)

updateSystem()
setInterval(updateSystem, 2000)
