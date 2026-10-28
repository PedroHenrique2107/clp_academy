#pragma once

#include <Arduino.h>  // PROGMEM

// Página servida em "/": espelha o LCD 16x2 (fonte 5x7 do HD44780 desenhada em
// <canvas>), mostra os 6 relés (K1..K6) e o status Wi-Fi/BLE. Consome /estado
// (polling a cada 1s, no mesmo ritmo do LCD físico) e /set (otimista: a UI já
// muda ao toque e se corrige com a resposta do servidor). Só fontes do
// sistema — o celular está no AP do CLP, sem internet para carregar um CDN.
//
// Arquivo próprio só para ter syntax highlighting de HTML/CSS/JS no editor;
// continua incluído e compilado dentro do mesmo firmware que main.cpp.
const char PAGINA[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="pt-br"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CLP HF-006</title>
<script>
// Aplica a preferência de tema salva antes do resto da página desenhar, pra não piscar.
(function(){try{var p=localStorage.getItem('clp-theme-pref');if(p==='light'||p==='dark')document.documentElement.setAttribute('data-theme',p);}catch(e){}})();
</script>
<style>
:root{
  --bg:#0e1117;--panel:#161a21;--raised:#1d222b;--line:#2a303b;
  --text:#e6e9ef;--dim:#8a92a3;--faint:#5a6273;--accent:#ff9a3c;
  --on:#3ddc78;--on-tint:rgba(61,220,120,.09);--on-line:rgba(61,220,120,.5);
  --danger:#ff6b6b;--danger-tint:rgba(255,107,107,.13);--danger-fill:rgba(255,107,107,.28);
  --lcd-bg:#1e44d0;--lcd-off:rgba(255,255,255,.075);--lcd-on:#eef3ff;--lcd-glow:rgba(170,195,255,.55);
  --mono:ui-monospace,"SF Mono",Consolas,"Roboto Mono",Menlo,monospace;
  --sans:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
  color-scheme:dark;
}
@media (prefers-color-scheme:light){:root:not([data-theme="dark"]){
  --bg:#ebe7dc;--panel:#f6f3ea;--raised:#fffdf7;--line:#d8d1bf;
  --text:#1f1b13;--dim:#6a6250;--faint:#a0987f;--accent:#c2600a;
  --on:#1d9a4b;--on-tint:rgba(29,154,75,.09);--on-line:rgba(29,154,75,.5);
  --danger:#b52a20;--danger-tint:rgba(181,42,32,.09);--danger-fill:rgba(181,42,32,.2);
  color-scheme:light;
}}
:root[data-theme="light"]{
  --bg:#ebe7dc;--panel:#f6f3ea;--raised:#fffdf7;--line:#d8d1bf;
  --text:#1f1b13;--dim:#6a6250;--faint:#a0987f;--accent:#c2600a;
  --on:#1d9a4b;--on-tint:rgba(29,154,75,.09);--on-line:rgba(29,154,75,.5);
  --danger:#b52a20;--danger-tint:rgba(181,42,32,.09);--danger-fill:rgba(181,42,32,.2);
  color-scheme:light;
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--text);font-family:var(--sans);padding:16px 16px 28px}
button{font:inherit;color:inherit}
.page{max-width:460px;margin:0 auto}
.mono{font-family:var(--mono)}
.label{font-family:var(--mono);font-size:10.5px;letter-spacing:.12em;text-transform:uppercase;color:var(--faint)}
header{display:flex;flex-wrap:wrap;align-items:flex-end;justify-content:space-between;gap:10px;margin-bottom:14px}
h1{font-family:var(--sans);font-size:21px;font-weight:800;margin:4px 0 0;letter-spacing:-.01em}
.chips{display:flex;flex-wrap:wrap;gap:6px}
.chip{display:inline-flex;align-items:center;gap:6px;font-family:var(--mono);font-size:11.5px;padding:5px 10px;border-radius:999px;background:var(--panel);border:1px solid var(--line);color:var(--dim)}
.chip b{color:var(--text);font-weight:700}
button.chip{appearance:none;-webkit-appearance:none;cursor:pointer}
.theme-btn svg{width:13px;height:13px;flex:none}
.dot{width:7px;height:7px;border-radius:50%;background:var(--faint);flex:none}
.dot.on{background:var(--on);box-shadow:0 0 6px var(--on)}
.dot.bad{background:var(--danger);box-shadow:0 0 6px var(--danger)}
.offline{display:flex;align-items:center;gap:8px;padding:9px 12px;border-radius:10px;background:var(--danger-tint);color:var(--danger);font-family:var(--mono);font-size:11.5px;margin-bottom:12px}
.lcd-card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:12px;margin-bottom:14px}
.lcd-head{display:flex;justify-content:space-between;align-items:baseline;gap:8px;margin-bottom:9px;flex-wrap:wrap}
.lcd-head .mono{font-size:11px;color:var(--dim)}
.lcd-module{position:relative;background:#0a0c10;border-radius:9px;padding:14px 18px}
#lcd{display:block;width:100%;height:auto;border-radius:3px}
.lcd-stale{position:absolute;right:16px;bottom:14px;font-family:var(--mono);font-size:10px;letter-spacing:.08em;padding:3px 7px;border-radius:4px;background:var(--danger);color:var(--lcd-on)}
.rack{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:12px}
.rack-head{display:flex;justify-content:space-between;align-items:baseline;gap:8px;margin-bottom:10px;flex-wrap:wrap}
.mask{font-family:var(--mono);font-size:13px;font-weight:700;color:var(--accent)}
.mask small{color:var(--dim);font-weight:400;margin-left:6px}
.tiles{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}
.tile{position:relative;display:grid;gap:9px;text-align:left;padding:10px 10px 11px;border-radius:11px;border:1px solid var(--line);background:var(--raised);cursor:pointer;transition:border-color .18s,background-color .18s,transform .08s,opacity .18s}
.tile:active{transform:scale(.97)}
.tile.on{border-color:var(--on-line);background-image:linear-gradient(var(--on-tint),var(--on-tint))}
.tile.pending{opacity:.55}
.tile:disabled{cursor:not-allowed;opacity:.4}
.tile-top{display:flex;justify-content:space-between;align-items:center}
.tile-k{font-family:var(--mono);font-size:11px;font-weight:700;color:var(--faint)}
.tile.on .tile-k{color:var(--on)}
.led{width:10px;height:10px;border-radius:50%;background:var(--line);box-shadow:inset 0 0 0 1px rgba(0,0,0,.25)}
.tile.on .led{background:var(--on);box-shadow:0 0 9px 1px var(--on)}
.tile-mid{display:flex;align-items:center;gap:8px}
.rocker{width:34px;height:19px;border-radius:5px;flex:none;background:var(--line);position:relative}
.rocker::after{content:"";position:absolute;top:2px;left:2px;width:15px;height:15px;border-radius:4px;background:var(--dim);transition:transform .16s,background-color .16s}
.tile.on .rocker::after{transform:translateX(15px);background:var(--on)}
.tile-state{font-family:var(--mono);font-size:14px;font-weight:800}
.tile.on .tile-state{color:var(--on)}
.actions{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:10px}
.btn{position:relative;overflow:hidden;border-radius:10px;padding:12px 10px;border:1px solid var(--line);background:var(--raised);font-family:var(--mono);font-size:12px;font-weight:700;letter-spacing:.05em;text-transform:uppercase;cursor:pointer}
.btn:disabled{opacity:.4;cursor:not-allowed}
.btn-on{color:var(--on);border-color:var(--on-line)}
.btn-off{color:var(--danger)}
.btn-off .fill{position:absolute;inset:0 auto 0 0;width:0;background:var(--danger-fill)}
.btn-off.arming .fill{width:100%;transition:width 1s linear}
.btn-off span{position:relative}
.hint{margin-top:7px;font-size:11px;color:var(--faint);text-align:center}
</style></head><body>
<div class="page">
<header>
  <div><div class="label">CLP HF-006 · 192.168.4.1</div><h1>Quadro de relés</h1></div>
  <div class="chips">
    <span class="chip"><span class="dot on" id="wifiDot"></span>Wi-Fi <b id="wifiChip">-</b></span>
    <span class="chip"><span class="dot" id="bleDot"></span>BLE <b id="bleChip">-</b></span>
    <button class="chip theme-btn" id="themeBtn" type="button">
      <svg id="themeIcon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"></svg>
      <span id="themeLabel">Auto</span>
    </button>
  </div>
</header>
<div class="offline" id="offline" hidden>&#9679; Sem resposta do CLP. Tentando reconectar...</div>
<section class="lcd-card">
  <div class="lcd-head"><span class="label">Display LCD 16&times;2</span><span class="mono" id="lcdTick">espelho em tempo real</span></div>
  <div class="lcd-module">
    <canvas id="lcd" width="518" height="116"></canvas>
    <span class="lcd-stale" id="lcdStale" hidden>ULTIMO ESTADO</span>
  </div>
</section>
<section class="rack">
  <div class="rack-head"><span class="label">Saidas K1-K6</span><span class="mask" id="mask">E:000000<small>0/6</small></span></div>
  <div class="tiles" id="tiles"></div>
  <div class="actions">
    <button class="btn btn-on" id="allOn">Ligar todas</button>
    <button class="btn btn-off" id="allOff"><i class="fill"></i><span>Segure p/ desligar</span></button>
  </div>
  <div class="hint">Toque num modulo para inverter. Para desligar todas, segure por 1s.</div>
</section>
</div>
<script>
(function(){
var TEMA_KEY='clp-theme-pref',TEMA_ORDEM=['system','light','dark'];
var TEMA_LABEL={system:'Auto',light:'Claro',dark:'Escuro'};
var TEMA_ICONE={
  system:'<rect x="3" y="4" width="18" height="12" rx="2"/><path d="M8 20h8M12 16v4"/>',
  light:'<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M2 12h2M20 12h2M6.34 17.66l-1.41 1.41M19.07 4.93l-1.41 1.41"/>',
  dark:'<path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/>'
};
function temaSalvo(){try{var v=localStorage.getItem(TEMA_KEY);return TEMA_ORDEM.indexOf(v)>=0?v:'system';}catch(e){return 'system';}}
var tema=temaSalvo();
function aplicarTema(t,salvar){
  tema=t;
  if(t==='system')document.documentElement.removeAttribute('data-theme');
  else document.documentElement.setAttribute('data-theme',t);
  document.getElementById('themeIcon').innerHTML=TEMA_ICONE[t];
  document.getElementById('themeLabel').textContent=TEMA_LABEL[t];
  if(salvar){try{localStorage.setItem(TEMA_KEY,t);}catch(e){}}
  if(lastGoodDraw)drawLCD(lastGoodDraw);   // recolore o LCD simulado com o tema novo
}
document.getElementById('themeBtn').addEventListener('click',function(){
  aplicarTema(TEMA_ORDEM[(TEMA_ORDEM.indexOf(tema)+1)%TEMA_ORDEM.length],true);
});
var FONT={'0':[14,17,19,21,25,17,14],'1':[4,12,4,4,4,4,14],'2':[14,17,1,2,4,8,31],'3':[31,2,4,2,1,17,14],
'4':[2,6,10,18,31,2,2],'5':[31,16,30,1,1,17,14],'6':[6,8,16,30,17,17,14],'7':[31,1,2,4,8,8,8],
'8':[14,17,17,14,17,17,14],'9':[14,17,17,15,1,2,12],
'I':[14,4,4,4,4,4,14],'N':[17,17,25,21,19,17,17],'O':[14,17,17,17,17,17,14],'U':[17,17,17,17,17,17,14],
'T':[31,4,4,4,4,4,4],'B':[30,17,17,30,17,17,30],'L':[16,16,16,16,16,16,31],'E':[31,16,16,30,16,16,31],
'W':[17,17,17,21,21,21,10],':':[0,12,12,0,12,12,0]};
var canvas=document.getElementById('lcd'),ctx=canvas.getContext('2d');
var DOT=4,PITCH=5,CELL_W=25,CELL_H=40,GAP_X=6,GAP_Y=8,PAD=14;
var dpr=Math.min(window.devicePixelRatio||1,3);
canvas.width=518*dpr;canvas.height=116*dpr;ctx.scale(dpr,dpr);
function css(n){return getComputedStyle(document.documentElement).getPropertyValue(n).trim();}
function pad16(s){s=s.slice(0,16);while(s.length<16)s+=' ';return s;}
function linhasLCD(st){
  var e=st.e,l0=pad16('IN  '+e+(st.ble?' BLE':'    ')),l1=pad16('OUT '+e+' W:'+st.w+' ');
  return [l0,l1];
}
function drawLCD(st){
  var bg=css('--lcd-bg'),off=css('--lcd-off'),on=css('--lcd-on'),glow=css('--lcd-glow');
  ctx.shadowBlur=0;ctx.fillStyle=bg;ctx.fillRect(0,0,518,116);
  var lines=linhasLCD(st);
  for(var r=0;r<2;r++){for(var c=0;c<16;c++){
    var g=FONT[lines[r][c]];
    var x0=PAD+c*(CELL_W+GAP_X),y0=PAD+r*(CELL_H+GAP_Y);
    for(var dy=0;dy<8;dy++){for(var dx=0;dx<5;dx++){
      var lit=g&&dy<7&&((g[dy]>>(4-dx))&1);
      ctx.fillStyle=lit?on:off;ctx.shadowColor=glow;ctx.shadowBlur=lit?5:0;
      ctx.fillRect(x0+dx*PITCH,y0+dy*PITCH,DOT,DOT);
    }}
  }}
}
var tilesEl=document.getElementById('tiles'),tiles=[];
for(var i=0;i<6;i++){(function(i){
  var b=document.createElement('button');
  b.className='tile';
  b.innerHTML='<div class="tile-top"><span class="tile-k">K'+(i+1)+'</span><span class="led"></span></div>'+
    '<div class="tile-mid"><span class="rocker"></span><span class="tile-state">OFF</span></div>';
  b.addEventListener('click',function(){enviar(i+1,'t');});
  tilesEl.appendChild(b);tiles.push(b);
})(i);}
var last={e:'000000',ble:false,w:0},offline=false,lastGoodDraw=null;
function render(st){
  var n=0;
  tiles.forEach(function(t,i){
    var on=st.e[i]==='1';if(on)n++;
    t.classList.toggle('on',on);
    t.disabled=offline;
    t.querySelector('.tile-state').textContent=on?'ON':'OFF';
  });
  document.getElementById('mask').innerHTML='E:'+st.e+'<small>'+n+'/6</small>';
  document.getElementById('wifiChip').textContent=offline?'-':st.w;
  document.getElementById('wifiDot').className='dot '+(offline?'bad':'on');
  document.getElementById('bleDot').className='dot'+(st.ble?' on':'');
  document.getElementById('bleChip').textContent=st.ble?'pareado':'livre';
  document.getElementById('offline').hidden=!offline;
  document.getElementById('lcdStale').hidden=!offline;
  document.getElementById('allOn').disabled=offline;
  document.getElementById('allOff').disabled=offline;
  if(!offline){drawLCD(st);lastGoodDraw=st;}
}
function aoReceber(j){
  falhasSeguidas=0;
  last={e:j.e,ble:!!j.ble,w:j.w||0};
  offline=false;render(last);
}
function enviar(e,v){
  if(offline)return;
  if(e!=='all'){var i=e-1;last.e=last.e.substr(0,i)+(v==='t'?(last.e[i]==='1'?'0':'1'):v)+last.e.substr(i+1);}
  else{last.e=v==='1'?'111111':'000000';}
  render(last);
  fetch('/set?e='+e+'&v='+v).then(function(r){return r.json();}).then(aoReceber).catch(registrarFalha);
}
function poll(){
  fetch('/estado').then(function(r){return r.json();}).then(aoReceber).catch(registrarFalha);
}
// O WebServer do ESP32 atende uma requisição por vez; sondagens de portal cativo do
// próprio celular (/generate_204 etc.) às vezes colidem com o polling e derrubam UM
// request isolado. Só marca offline depois de falhas seguidas, pra não piscar o banner
// à toa enquanto tudo continua funcionando (relés, etc.).
var falhasSeguidas=0,OFFLINE_APOS=2;
function registrarFalha(){
  falhasSeguidas++;
  if(falhasSeguidas>=OFFLINE_APOS){offline=true;render(last);}
}
document.getElementById('allOn').addEventListener('click',function(){enviar('all','1');});
var offBtn=document.getElementById('allOff'),hold=null;
function armar(){if(offline)return;offBtn.classList.add('arming');hold=setTimeout(function(){desarmar();enviar('all','0');},1000);}
function desarmar(){clearTimeout(hold);offBtn.classList.remove('arming');}
offBtn.addEventListener('pointerdown',armar);
['pointerup','pointerleave','pointercancel'].forEach(function(ev){offBtn.addEventListener(ev,desarmar);});
offBtn.addEventListener('contextmenu',function(e){e.preventDefault();});
var tick=0;
setInterval(function(){
  poll();
  tick=(tick+1)%2;
  document.getElementById('lcdTick').textContent=offline?'congelado':(tick?'atualizado ●':'atualizado ○');
},1000);
matchMedia('(prefers-color-scheme: dark)').addEventListener('change',function(){if(lastGoodDraw)drawLCD(lastGoodDraw);});
aplicarTema(tema,false);
poll();
})();
</script>
</body></html>
)rawliteral";
