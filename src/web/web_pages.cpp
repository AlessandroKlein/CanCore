#include "web/web_pages.h"

namespace pcd {

namespace {

const char kWebAppHtml[] = R"PCDWEB(<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PCD Gateway</title>
<style>
:root{--ink:#17212b;--muted:#64727e;--paper:#f4f1ea;--panel:#fffdf8;--line:#d8d1c5;--accent:#0c7c86;--hot:#e06c47;--ok:#2f8f68;--shadow:0 14px 35px #17212b16}*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at 15% 0,#e4f0ed 0,transparent 32%),var(--paper);color:var(--ink);font:15px/1.5 Georgia,serif}header{display:flex;align-items:center;justify-content:space-between;gap:20px;padding:24px clamp(18px,5vw,72px);border-bottom:1px solid var(--line)}h1,h2,h3,p{margin-top:0}h1{font-size:clamp(28px,5vw,52px);line-height:1;margin-bottom:8px;letter-spacing:0}header p{margin:0;color:var(--muted)}nav{display:flex;flex-wrap:wrap;gap:8px;padding:16px clamp(18px,5vw,72px);background:#fffaf0;border-bottom:1px solid var(--line)}nav a{padding:8px 12px;color:var(--ink);text-decoration:none;border:1px solid transparent}nav a:hover,nav a.active{border-color:var(--accent);color:var(--accent)}main{max-width:1250px;margin:0 auto;padding:28px clamp(18px,5vw,72px) 60px}.view{display:none}.view.active{display:block}.eyebrow{color:var(--hot);font:700 12px/1.2 monospace;letter-spacing:1px;text-transform:uppercase}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:14px}.panel{background:var(--panel);border:1px solid var(--line);box-shadow:var(--shadow);padding:20px}.metric{font-size:34px;font-weight:700;color:var(--accent)}.muted{color:var(--muted)}.status{display:inline-flex;align-items:center;gap:7px;color:var(--ok);font-weight:700}.status:before{content:"";width:9px;height:9px;border-radius:50%;background:currentColor}.table{width:100%;border-collapse:collapse;background:var(--panel)}.table th,.table td{text-align:left;padding:11px;border-bottom:1px solid var(--line)}.table th{font:700 12px monospace;text-transform:uppercase;color:var(--muted)}button{border:0;background:var(--accent);color:white;padding:10px 15px;font:700 14px monospace;cursor:pointer}button.secondary{background:var(--ink)}code{font-family:monospace;color:var(--accent)}footer{padding:20px clamp(18px,5vw,72px);color:var(--muted);border-top:1px solid var(--line)}
</style>
</head>
<body>
<header><div><div class="eyebrow">PCD / CAN 2.0B</div><h1>Gateway control</h1><p>Operacion, diagnostico y actualizacion del ecosistema.</p></div><div class="status" id="connection">conectando</div></header>
<nav id="nav"><a href="#dashboard">Resumen</a><a href="#nodes">Nodos</a><a href="#routes">Rutas</a><a href="#ota">OTA</a><a href="#tunnel">Tunel</a><a href="#diagnostics">Diagnostico</a><a href="#settings">Ajustes</a></nav>
<main>
<section class="view" id="view-dashboard"><div class="eyebrow">Operacion</div><h2>Resumen del gateway</h2><div class="grid"><article class="panel"><div class="muted">Nodos activos</div><div class="metric" id="metric-nodes">--</div></article><article class="panel"><div class="muted">Tramas por minuto</div><div class="metric" id="metric-traffic">--</div></article><article class="panel"><div class="muted">Estado CAN</div><div class="metric" id="metric-can">--</div></article></div><br><article class="panel"><h3>Actividad reciente</h3><table class="table"><thead><tr><th>Hora</th><th>Origen</th><th>Tipo</th><th>Detalle</th></tr></thead><tbody id="activity"><tr><td colspan="4" class="muted">Esperando datos...</td></tr></tbody></table></article></section>
<section class="view" id="view-nodes"><div class="eyebrow">Inventario</div><h2>Nodos del bus</h2><article class="panel"><table class="table"><thead><tr><th>Node ID</th><th>Heartbeat</th><th>Recursos</th><th>Estado</th></tr></thead><tbody id="nodes"><tr><td colspan="4" class="muted">Esperando datos...</td></tr></tbody></table></article></section>
<section class="view" id="view-routes"><div class="eyebrow">Enrutamiento</div><h2>Tabla de rutas</h2><article class="panel"><p class="muted">Las rutas se almacenan en el gateway y conectan CAN con MQTT, Modbus o tuneles.</p><table class="table"><thead><tr><th>Origen</th><th>Filtro</th><th>Destino</th><th>Estado</th></tr></thead><tbody id="routes"><tr><td colspan="4" class="muted">Esperando datos...</td></tr></tbody></table></article></section>
<section class="view" id="view-ota"><div class="eyebrow">Mantenimiento</div><h2>Actualizacion OTA por CAN</h2><div class="grid"><article class="panel"><h3>Campana activa</h3><div class="metric" id="ota-progress">0%</div><p id="ota-status" class="muted">Sin transferencia.</p><button class="secondary" id="ota-abort">Abortar</button></article><article class="panel"><h3>Contrato</h3><p>El gateway envia bloques de 7 bytes con CRC-16 global. El bootloader receptor debe confirmar START, ventanas y FINISH.</p><code>/api/ota/status</code></article></div></section>
<section class="view" id="view-tunnel"><div class="eyebrow">Interconexion</div><h2>Tunel CAN sobre IP</h2><div class="grid"><article class="panel"><div class="muted">Transporte</div><div class="metric" id="tunnel-transport">--</div><p id="tunnel-peer" class="muted">Sin peers.</p></article><article class="panel"><div class="muted">Datagramas</div><div class="metric" id="tunnel-datagrams">--</div><p class="muted">Codec de 16 bytes y CRC-16.</p></article></div></section>
<section class="view" id="view-diagnostics"><div class="eyebrow">Salud del sistema</div><h2>Diagnostico</h2><article class="panel"><table class="table"><tbody id="diagnostics"><tr><td>Consultando...</td></tr></tbody></table></article></section>
<section class="view" id="view-settings"><div class="eyebrow">Configuracion</div><h2>Ajustes del gateway</h2><div class="grid"><article class="panel"><h3>Identidad del nodo</h3><p class="muted">Manual conserva el ID indicado. Automatico deriva un ID estable desde la identidad de hardware.</p><label>Modo <select id="identity-mode"><option value="0">Manual</option><option value="1">Automatico</option></select></label><br><label>Node-ID <input id="identity-id" type="number" min="1" max="16383"></label><br><button id="identity-save">Guardar identidad</button></article><article class="panel"><h3>Filtro de escucha</h3><p class="muted">Controla que estados se entregan a los suscriptores del gateway.</p><label>Origen <input id="filter-source" type="number" min="0" max="16383" value="0"></label><br><label>Recurso <input id="filter-resource" type="number" min="0" max="255" value="64"></label><br><label>Canal <input id="filter-channel" type="number" min="0" max="255" value="255"></label><br><button id="filter-save">Agregar filtro</button></article></div><br><article class="panel"><p class="muted">La API concreta debe autenticar estas operaciones antes de modificar el bus.</p><table class="table"><tbody><tr><th>GET</th><td><code>/api/identity</code>, <code>/api/listen-filters</code></td></tr><tr><th>POST</th><td><code>/api/identity</code>, <code>/api/listen-filters</code></td></tr></tbody></table></article></section>
</main><footer>PCD_CAN &middot; interfaz de referencia para proyectos gateway</footer>
<script>
const views=['dashboard','nodes','routes','ota','tunnel','diagnostics','settings'];
function show(){const name=(location.hash||'#dashboard').slice(1);const active=views.includes(name)?name:'dashboard';views.forEach(v=>{document.getElementById('view-'+v).classList.toggle('active',v===active)});document.querySelectorAll('#nav a').forEach(a=>a.classList.toggle('active',a.hash==='#'+active));}
async function json(url,fallback){try{const r=await fetch(url);if(!r.ok)throw Error(r.status);return await r.json()}catch(e){return fallback}}
function resourceName(value){const names={16:'Rele',32:'Dimmer',48:'Entrada digital',64:'Sensor ambiental',80:'Sensor de gas',96:'Sensor electrico',112:'Cortina',224:'Configuracion',240:'Sistema'};return names[value]||('Recurso 0x'+Number(value).toString(16).toUpperCase())}
async function refreshNodes(){const data=await json('/api/nodes',{nodes:[]});const rows=(data.nodes||[]).map(n=>'<tr><td>'+n.id+'</td><td>'+n.last_heartbeat+'</td><td>'+((n.resources||[]).map(r=>resourceName(r.type)+' / '+r.channel).join(', ')||'Sin anunciar')+'</td><td>'+n.status+'</td></tr>').join('');document.getElementById('nodes').innerHTML=rows||'<tr><td colspan="4" class="muted">Sin nodos anunciados</td></tr>'}
async function refreshIdentity(){const data=await json('/api/identity',{});if(data.mode!==undefined)document.getElementById('identity-mode').value=data.mode;if(data.id!==undefined)document.getElementById('identity-id').value=data.id}
async function postJson(url,body){return fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})}
async function refresh(){const s=await json('/api/summary',{});document.getElementById('metric-nodes').textContent=s.nodes===undefined?'--':s.nodes;document.getElementById('metric-traffic').textContent=s.frames_per_minute===undefined?'--':s.frames_per_minute;document.getElementById('metric-can').textContent=s.can_status===undefined?'--':s.can_status;document.getElementById('connection').textContent='gateway conectado';refreshNodes();refreshIdentity()}
document.getElementById('identity-save').onclick=()=>postJson('/api/identity',{mode:Number(document.getElementById('identity-mode').value),id:Number(document.getElementById('identity-id').value)});
document.getElementById('filter-save').onclick=()=>postJson('/api/listen-filters',{source:Number(document.getElementById('filter-source').value),resource:Number(document.getElementById('filter-resource').value),channel:Number(document.getElementById('filter-channel').value)});
window.addEventListener('hashchange',show);show();refresh();setInterval(refresh,5000);
</script></body></html>)PCDWEB";

const char *kNames[WEB_VIEW_COUNT] = {"Resumen", "Nodos", "Rutas", "OTA", "Tunel", "Diagnostico", "Ajustes"};
const char *kPaths[WEB_VIEW_COUNT] = {"/", "/#nodes", "/#routes", "/#ota", "/#tunnel", "/#diagnostics", "/#settings"};

}  // namespace

const char *webAppHtml() { return kWebAppHtml; }

const char *webViewName(WebView view) {
    return view < WEB_VIEW_COUNT ? kNames[view] : "Desconocida";
}

const char *webViewPath(WebView view) {
    return view < WEB_VIEW_COUNT ? kPaths[view] : "/";
}

}  // namespace pcd
