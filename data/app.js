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
