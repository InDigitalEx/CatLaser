const $=id=>document.getElementById(id);
const MODES=[
 [1,'↔️','Маятник','Плавно туда-сюда',0],
 [2,'🔲','Растр','Строчками, как сканер',1],
 [3,'🎲','Хаос','Случайные точки',0],
 [4,'🐭','Охота','Крадётся, замирает, рывок',0],
 [5,'🦋','Бабочка','Плавное порхание',0],
 [6,'♾️','Восьмёрка','Бесконечность',1],
 [7,'🌀','Спираль','Круги то шире, то уже',1],
 [8,'👣','Прыжки','Шажки в разные стороны',0],
 [9,'🕳️','Норка','Мышка выглядывает',0],
 [10,'🎱','Рикошет','Отскакивает от границ',0],
 [11,'😵','Нервный','Рывки, развороты, замирания',0],
 [12,'🌊','Волна','Размах то растёт, то падает',0],
 [13,'🚀','Разгон','Каждый проход быстрее',0],
 [14,'🔔','Затухание','Как раскачанный маятник',0],
 [17,'🪰','Муха','Мельтешит у одной точки',0],
 [18,'⚡','Молния','Серия быстрых зигзагов',0],
 [19,'🥁','Ритм','Прыжки по углам в такт',0],
 [20,'⭐','Звезда','Рисует пентаграмму',1],
 [21,'🛤️','Периметр','Бегает по границе зоны',1],
 [22,'🐍','Змейка','Волна поперёк зоны',1],
 [23,'🌪️','Вихрь','Быстрый кружок блуждает',1],
 [15,'🎰','Авто','Сам меняет режимы',0],
 [16,'🎮','Ручной','Води пальцем',0],
 [0,'⏹','Стоп','Всё выключить',0]
];
const NAMES={};MODES.forEach(m=>NAMES[m[0]]=m[2]);
const LASERS=[[1,'🔴 Горит'],[2,'💡 Мигает'],[3,'💓 Пульс'],[4,'⚡ Строб'],[5,'🔥 Мерцает'],[8,'💗 Сердце'],[9,'🆘 SOS'],[6,'🧊 Только на паузах'],[7,'🏃 Только в движении'],[10,'👀 Мигает на паузах'],[11,'🔦 Мигает в движении'],[0,'🚫 Выкл']];
let S={};

function toast(t){const e=$('toast');e.textContent=t;e.className='show';clearTimeout(toast.t);toast.t=setTimeout(()=>e.className='',1800)}
function api(q){return fetch('/api/set?'+q).then(r=>{if(r.status===409)toast('Этот режим работает только с двумя серво');return r.json()}).then(render).catch(()=>toast('Нет связи'))}
function deb(f,ms){let t;return(...a)=>{clearTimeout(t);t=setTimeout(()=>f(...a),ms)}}

MODES.forEach(m=>{
  const b=document.createElement('button');b.className='m'+(m[4]?' need2':'');b.dataset.m=m[0];
  b.innerHTML=(m[4]?'<span class="tag">2 оси</span>':'')+'<span class="ic">'+m[1]+'</span><b>'+m[2]+'</b><span class="d">'+m[3]+'</span>';
  b.onclick=()=>api('mode='+m[0]);
  $('modes').appendChild(b);
});
LASERS.forEach(l=>{
  const b=document.createElement('button');b.className='lc';b.dataset.l=l[0];b.textContent=l[1];
  b.onclick=()=>api('lmode='+l[0]);
  $('lasers').appendChild(b);
});

function drawField(){
  const pen=S.pen!==0,ten=S.ten!==0;
  const a=pen?+$('xmin').value:0,b=pen?+$('xmax').value:180;
  const c=ten?+$('ymin').value:0,d=ten?+$('ymax').value:180;
  const x=S.x??90,y=ten?(S.y??90):90;
  $('field').innerHTML=
   '<rect x="0" y="0" width="180" height="180" rx="10" fill="#14111f"/>'+
   '<path d="M45 0V180M90 0V180M135 0V180M0 45H180M0 90H180M0 135H180" stroke="#2c2747" stroke-width="1"/>'+
   '<rect x="'+a+'" y="'+(180-d)+'" width="'+(b-a)+'" height="'+(d-c)+'" fill="#ff7a4522" stroke="#ff7a45" stroke-width="2" rx="4"/>'+
   (S.mode&&(pen||ten)?'<circle cx="'+(pen?x:90)+'" cy="'+(180-y)+'" r="5" fill="#ff4d6d" stroke="#fff" stroke-width="1.5"/>':'');
}

function labels(){
  $('vXmin').textContent=$('xmin').value+'°';
  $('vXmax').textContent=$('xmax').value+'°';
  $('vYmin').textContent=$('ymin').value+'°';
  $('vYmax').textContent=$('ymax').value+'°';
  $('vSpeed').textContent=$('speed').value+' / 10';
  $('vPmin').textContent=(+$('pmin').value).toFixed(1)+' с';
  $('vPmax').textContent=(+$('pmax').value).toFixed(1)+' с';
  $('vAuto').textContent=$('auto').value+' с';
  $('vTimer').textContent=$('timer').value==0?'выкл':$('timer').value+' мин';
  $('vLbr').textContent=$('lbr').value+'%';
  $('vLrate').textContent=$('lrate').value+' / 10';
  $('vRmin').textContent=$('rmin').value+' мин';
  $('vRmax').textContent=$('rmax').value+' мин';
  $('vPlay').textContent=$('play').value+' мин';
  drawField();
}

function setIf(id,v,chk){const e=$(id);if(document.activeElement!==e){chk?e.checked=!!v:e.value=v}}

let drag=false;
const clamp=v=>Math.min(1,Math.max(0,v));
function placeDot(){
  if(drag||S.xmin===undefined)return;
  const two=S.pen&&S.ten,st=$('padDot').style;
  if(two){
    st.left=clamp((S.mx-S.xmin)/Math.max(1,S.xmax-S.xmin))*100+'%';
    st.top=clamp((S.ymax-S.my)/Math.max(1,S.ymax-S.ymin))*100+'%';
  }else{
    const f=S.pen?(S.mx-S.xmin)/Math.max(1,S.xmax-S.xmin):(S.my-S.ymin)/Math.max(1,S.ymax-S.ymin);
    st.left=clamp(f)*100+'%';st.top='50%';
  }
}

function render(s){
  S=s;
  const two=s.pen&&s.ten;
  document.querySelectorAll('.m').forEach(b=>{
    b.classList.toggle('sel',+b.dataset.m===s.mode);
    b.disabled=b.classList.contains('need2')&&!two;   // режимы для 2 осей недоступны при одной серво
  });
  document.querySelectorAll('.lc').forEach(b=>b.classList.toggle('sel',+b.dataset.l===s.lmode));
  setIf('pen',s.pen,1);setIf('ten',s.ten,1);setIf('invx',s.ix,1);setIf('invy',s.iy,1);
  setIf('xmin',s.xmin);setIf('xmax',s.xmax);setIf('ymin',s.ymin);setIf('ymax',s.ymax);
  setIf('speed',s.speed);setIf('pmin',s.pmin/1000);setIf('pmax',s.pmax/1000);
  setIf('auto',s.auto);setIf('timer',s.timer);setIf('lbr',s.lbr);setIf('lrate',s.lrate);
  setIf('astart',s.astart,1);
  setIf('se',s.se,1);setIf('rmin',s.rmin);setIf('rmax',s.rmax);setIf('play',s.play);setIf('smode',s.smode);
  sessStatus(s);
  behaviorRender(s);
  ['rXmin','rXmax'].forEach(i=>$(i).classList.toggle('hide',!s.pen));
  ['rYmin','rYmax'].forEach(i=>$(i).classList.toggle('hide',!s.ten));
  $('warn').classList.toggle('hide',s.pen||s.ten);
  $('manCard').classList.toggle('hide',s.mode!==16);
  $('pad').classList.toggle('strip',!two);
  $('padHint').textContent=two?'Води пальцем по полю — лазер идёт за ним в пределах зоны игры.':'Води пальцем по полосе — лазер идёт за ним в пределах зоны игры.';
  $('rowAuto').classList.toggle('hide',s.mode!==15);
  $('chip').className='chip'+(s.mode?' on':'');
  let t=NAMES[s.mode];
  if(s.mode===15&&s.run)t+=' · '+NAMES[s.run];
  if(s.mode&&s.left>0){const m=Math.floor(s.left/60),c=s.left%60;t+=' · '+m+':'+String(c).padStart(2,'0')}
  if(s.finale)t+=' · 🏁 финал';
  $('st').textContent=t;
  $('info').textContent='IP '+s.ip+' · WiFi '+s.rssi+' dBm · catlaser.local';
  mqStatus(s);
  labels();placeDot();
}

const send=deb(q=>api(q),120);
['xmin','xmax','ymin','ymax','speed','auto','timer','lbr','lrate','rmin','rmax','play'].forEach(id=>{
  $(id).oninput=()=>{labels();send(id+'='+$(id).value)};
});
['pmin','pmax'].forEach(id=>{
  $(id).oninput=()=>{labels();send(id+'='+Math.round($(id).value*1000))};
});
const tog=(id,key)=>{$(id).onchange=()=>api(key+'='+($(id).checked?1:0))};
tog('pen','pen');tog('ten','ten');tog('invx','ix');tog('invy','iy');tog('astart','astart');tog('se','se');

// --- дежурный режим ---
const sm=$('smode');
sm.innerHTML=MODES.filter(m=>m[0]!==0).map(m=>'<option value="'+m[0]+'">'+m[1]+' '+m[2]+'</option>').join('');
sm.onchange=()=>api('smode='+sm.value);
function fmtT(sec){return sec>=90?Math.round(sec/60)+' мин':sec+' с'}
function sessStatus(s){
  let t;
  if(!s.se)t='⚪ выключено';
  else if(s.mode)t='🟢 идёт игра';
  else if(s.snext>=0)t='⏳ следующая игра через '+fmtT(s.snext);
  else t='…';
  $('sst').textContent=t;
}

// --- MQTT ---
const RC={'-4':'нет ответа от брокера','-3':'соединение потеряно','-2':'не удалось подключиться','-1':'отключено','1':'неверная версия протокола','2':'неверный ID клиента','3':'брокер недоступен','4':'неверный логин или пароль','5':'нет авторизации'};
const mon=$('mqon');
mon.innerHTML='<option value="0">Последний использованный</option>'+MODES.filter(m=>m[0]!==0).map(m=>'<option value="'+m[0]+'">'+m[1]+' '+m[2]+'</option>').join('');
function mqHint(){
  const t=$('mqtopic').value||'home/catlaser/power';
  $('mqhint').textContent='ON или 1 — включить, OFF или 0 — выключить. Состояние: '+t+'/state, доступность: '+t+'/lwt. Дежурный режим: '+t+'/auto (состояние '+t+'/auto/state). Не ставь retain на команды, иначе игрушка запустится сразу при включении.';
}
function mqStatus(s){
  let t;
  if(!s.mqen)t='⚪ выключено';
  else if(s.mqok)t='🟢 подключено';
  else t='🟠 подключаюсь… '+(RC[s.mqrc]||'');
  $('mqst').textContent=t;
}
function loadMq(){
  fetch('/api/mqtt').then(r=>r.json()).then(c=>{
    $('mqen').checked=!!c.en;$('mqhost').value=c.host;$('mqport').value=c.port;
    $('mquser').value=c.user;$('mqpass').value='';$('mqclr').checked=false;
    $('mqpass').placeholder=c.haspass?'••••••• (сохранён)':'пароль';
    $('mqtopic').value=c.topic;$('mqon').value=c.onmode;mqHint();
  }).catch(()=>{});
}
$('mqtopic').oninput=mqHint;
$('mqsave').onclick=()=>{
  const b=new URLSearchParams();
  b.set('en',$('mqen').checked?1:0);
  b.set('host',$('mqhost').value);
  b.set('port',$('mqport').value||1883);
  b.set('user',$('mquser').value);
  if($('mqpass').value)b.set('pass',$('mqpass').value);
  if($('mqclr').checked)b.set('clrpass',1);
  b.set('topic',$('mqtopic').value);
  b.set('onmode',$('mqon').value);
  fetch('/api/mqtt',{method:'POST',body:b}).then(r=>{if(!r.ok)throw 0;return r.json()}).then(()=>{toast('MQTT сохранён ✓');loadMq()}).catch(()=>toast('Ошибка: проверь топик (без # и +)'));
};

// --- пад для ручного режима (2D поле или 1D полоса) ---
const pad=$('pad');let lastPos=0;
function sendPos(q){
  const n=Date.now();
  clearTimeout(sendPos.t);
  if(n-lastPos>70){lastPos=n;api(q)}else{sendPos.t=setTimeout(()=>api(q),80)}
}
function padMove(e){
  const r=pad.getBoundingClientRect();
  const fx=clamp((e.clientX-r.left)/r.width),fy=clamp((e.clientY-r.top)/r.height);
  const two=S.pen&&S.ten,st=$('padDot').style;
  st.left=fx*100+'%';
  if(two){
    st.top=fy*100+'%';
    sendPos('mx='+Math.round(S.xmin+fx*(S.xmax-S.xmin))+'&my='+Math.round(S.ymax-fy*(S.ymax-S.ymin)));
  }else if(S.pen){
    sendPos('mx='+Math.round(S.xmin+fx*(S.xmax-S.xmin)));
  }else if(S.ten){
    sendPos('my='+Math.round(S.ymin+fx*(S.ymax-S.ymin)));
  }
}
pad.onpointerdown=e=>{drag=true;pad.setPointerCapture(e.pointerId);padMove(e)};
pad.onpointermove=e=>{if(drag)padMove(e)};
pad.onpointerup=pad.onpointercancel=()=>{drag=false};

$('wifi').onclick=()=>{
  if(confirm('Сбросить WiFi? Устройство перезагрузится и откроет точку доступа CatLaser-Setup.')){
    fetch('/api/wifi-reset');toast('Перезагрузка...');
  }
};

function poll(){if(document.hidden||drag)return;fetch('/api/state').then(r=>r.json()).then(render).catch(()=>{})}
// ===================== Поведение игры, калибровка, статистика =====================
const WPOOL=MODES.filter(m=>![0,15,16].includes(m[0]));
const wl=$('wlist');
WPOOL.forEach(m=>{
  const d=document.createElement('div');d.className='row';
  d.innerHTML='<div class="lab"><span>'+m[1]+' '+m[2]+'</span><em id="wv'+m[0]+'">1</em></div><input type="range" id="w'+m[0]+'" min="0" max="10">';
  wl.appendChild(d);
});
function wtsCsv(){
  const a=[];for(let i=0;i<24;i++){const e=$('w'+i);a.push(e?e.value:1)}return a.join(',');
}
const sendW=deb(()=>api('wts='+wtsCsv()),200);
WPOOL.forEach(m=>{$('w'+m[0]).oninput=()=>{$('wv'+m[0]).textContent=$('w'+m[0]).value;sendW()}});

function labels2(){
  $('vSacc').textContent=$('sacc').value+' / 10';
  $('vRpct').textContent=$('rpct').value+'%';
  $('vFinsec').textContent=$('finsec').value+' с';
  ['0','1'].forEach(i=>{
    $('vMn'+i).textContent=$('mn'+i).value+' мкс';
    $('vMx'+i).textContent=$('mx'+i).value+' мкс';
    const o=+$('of'+i).value;$('vOf'+i).textContent=(o>0?'+':'')+o+'°';
  });
}
['sacc','rpct','finsec','mn0','mx0','of0','mn1','mx1','of1'].forEach(id=>{
  $(id).oninput=()=>{labels2();send(id+'='+$(id).value)};
});
tog('sm','sm');tog('ramp','ramp');tog('fin','fin');tog('plist','plist');tog('sten','sten');

document.querySelectorAll('.tests .b').forEach(b=>{
  b.onclick=()=>{const [ax,pos]=b.dataset.t.split(':');
    fetch('/api/servotest?axis='+ax+'&pos='+pos).then(r=>r.json()).then(render).catch(()=>toast('Нет связи'))};
});

function behaviorRender(s){
  setIf('sm',s.sm,1);setIf('sacc',s.sacc);
  setIf('ramp',s.ramp,1);setIf('rpct',s.rpct);
  setIf('fin',s.fin,1);setIf('finsec',s.finsec);
  setIf('plist',s.plist,1);setIf('sten',s.sten,1);
  ['mn0','mx0','of0','mn1','mx1','of1'].forEach(k=>setIf(k,s[k]));
  $('rSacc').classList.toggle('dim',!s.sm);
  $('rRpct').classList.toggle('dim',!s.ramp);
  $('rFinsec').classList.toggle('dim',!s.fin);
  wl.classList.toggle('hide',!s.plist);
  WPOOL.forEach(m=>{setIf('w'+m[0],s.wts[m[0]]);$('wv'+m[0]).textContent=$('w'+m[0]).value});
  setIf('tz',s.tz);
  const ts=$('tzsel');if(document.activeElement!==ts)ts.value=(TZS.find(t=>t[1]===s.tz)||['',''])[1];
  $('statsBody').classList.toggle('dim',!s.sten);
  labels2();
}

// --- статистика ---
const TZS=[['Хельсинки / Киев','EET-2EEST,M3.5.0/3,M10.5.0/4'],['Москва','MSK-3'],['Берлин / Амстердам / Париж','CET-1CEST,M3.5.0,M10.5.0/3'],['Лондон','GMT0BST,M3.5.0/1,M10.5.0'],['UTC','UTC0'],['Другой (вручную)','']];
$('tzsel').innerHTML=TZS.map(t=>'<option value="'+t[1]+'">'+t[0]+'</option>').join('');
$('tzsel').onchange=()=>{const v=$('tzsel').value;if(v){$('tz').value=v;api('tz='+encodeURIComponent(v));setTimeout(loadStats,1500)}else $('tz').focus()};
$('tz').onchange=()=>{if($('tz').value.trim()){api('tz='+encodeURIComponent($('tz').value.trim()));setTimeout(loadStats,1500)}};

function fmtDur(sec){
  if(sec<60)return sec+' с';
  const m=Math.round(sec/60);
  if(m<60)return m+' мин';
  return Math.floor(m/60)+' ч '+(m%60)+' мин';
}
const WD=['Вс','Пн','Вт','Ср','Чт','Пт','Сб'];
function renderStats(st){
  $('kToday').textContent=st.sync?fmtDur(st.today):'—';
  $('kWeek').textContent=st.sync?fmtDur(st.week):'—';
  $('kTotal').textContent=fmtDur(st.total);
  $('kSess').textContent=st.sessions;
  const max=Math.max(60,...st.days);
  $('bars').innerHTML=st.days.map((s,i)=>
    '<div class="bar'+(i===6?' today':'')+'"><i style="height:'+Math.round(s/max*80)+'px"></i>'+(st.sync?WD[st.wd[i]]:'·')+'</div>').join('');
  $('stNote').textContent=st.sync?('Время на устройстве: '+st.clock+' · по оси — минуты игры за день'):'Время ещё не получено по интернету (NTP): статистика по дням появится, когда игрушка подключится к сети. Общее время и режимы считаются уже сейчас.';
  const rows=st.run.map((s,i)=>[i,s,st.cnt[i]]).filter(r=>r[1]>0).sort((a,b)=>b[1]-a[1]).slice(0,6);
  const top=rows.length?rows[0][1]:1;
  $('topmodes').innerHTML=rows.length?rows.map(r=>
    '<div class="tm"><div class="lab"><span>'+(NAMES[r[0]]||('режим '+r[0]))+'</span><em>'+fmtDur(r[1])+' · '+r[2]+'×</em></div><div class="track"><i style="width:'+Math.round(r[1]/top*100)+'%"></i></div></div>').join('')
    :'<div class="hint">Пока нет данных — запусти игру.</div>';
}
function loadStats(){fetch('/api/stats').then(r=>r.json()).then(renderStats).catch(()=>{})}
$('stReset').onclick=()=>{
  if(confirm('Сбросить всю статистику?'))fetch('/api/stats/reset',{method:'POST'}).then(r=>r.json()).then(st=>{renderStats(st);toast('Статистика сброшена')});
};

loadMq();loadStats();poll();setInterval(poll,1200);setInterval(()=>{if(!document.hidden)loadStats()},15000);
