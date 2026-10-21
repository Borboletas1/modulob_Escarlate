// pagina.h
const char PAGINA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>VaxGuard Connect</title>
  <style>
    body { font-family: Arial, sans-serif; background: #f0f2f5; margin: 0; padding: 20px; }
    .container { max-width: 600px; margin: auto; background: white; border-radius: 12px; padding: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    h1 { text-align: center; color: #333; font-size: 1.4rem; }
    .card { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-top: 20px; }
    .item { background: #f8f9fa; padding: 14px; border-radius: 8px; text-align: center; }
    .item .label { font-size: 0.75rem; color: #666; text-transform: uppercase; }
    .item .valor { font-size: 1.4rem; font-weight: bold; color: #222; margin-top: 4px; }
    .estado-box { text-align: center; padding: 16px; border-radius: 8px; margin-top: 16px; font-weight: bold; font-size: 1.1rem; }
    .normal  { background: #d4edda; color: #155724; }
    .atencao { background: #fff3cd; color: #856404; }
    .critico { background: #f8d7da; color: #721c24; }
    .reconhecido { background: #ffe0b2; color: #e65100; }
    .falha   { background: #e2e3e5; color: #383d41; }
    .status-rede { text-align: center; font-size: 0.8rem; color: #888; margin-top: 10px; }
    .erro { color: red; font-size: 0.8rem; text-align: center; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🧪 VaxGuard Connect</h1>

    <div class="card">
      <div class="item">
        <div class="label">Temperatura</div>
        <div class="valor" id="temp">--</div>
      </div>
      <div class="item">
        <div class="label">Umidade</div>
        <div class="valor" id="umid">--</div>
      </div>
      <div class="item">
        <div class="label">Porta</div>
        <div class="valor" id="porta">--</div>
      </div>
      <div class="item">
        <div class="label">Luz (LDR)</div>
        <div class="valor" id="luz">--</div>
      </div>
    </div>

    <div class="estado-box normal" id="estado">Carregando...</div>
    <div class="status-rede" id="rede">Conectando ao Arduino...</div>
    <div class="erro" id="erro"></div>
  </div>

  <script>
    const estadoMap = {
      0: { texto: "NORMAL",      classe: "normal" },
      1: { texto: "ATENÇÃO",     classe: "atencao" },
      2: { texto: "CRÍTICO",     classe: "critico" },
      3: { texto: "RECONHECIDO", classe: "reconhecido" },
      4: { texto: "FALHA SENSOR",classe: "falha" }
    };

    async function buscarDados() {
      try {
        const resp = await fetch('/dados', { cache: 'no-store' });
        if (!resp.ok) throw new Error("HTTP " + resp.status);
        const d = await resp.json();

        document.getElementById('temp').textContent  = d.temp.toFixed(1) + " °C";
        document.getElementById('umid').textContent  = d.umid.toFixed(0) + " %";
        document.getElementById('porta').textContent = d.porta ? "ABERTA" : "FECHADA";
        document.getElementById('luz').textContent   = d.luz;

        const est = estadoMap[d.estado] || { texto: "?", classe: "falha" };
        const el  = document.getElementById('estado');
        el.textContent = est.texto;
        el.className   = "estado-box " + est.classe;

        document.getElementById('rede').textContent =
          "🟢 Atualizado: " + new Date().toLocaleTimeString();
        document.getElementById('erro').textContent = "";
      } catch (e) {
        document.getElementById('rede').textContent = "🔴 Sem conexão com o Arduino";
        document.getElementById('erro').textContent = e.message;
      }
    }

    setInterval(buscarDados, 3000);
    buscarDados();
  </script>
</body>
</html>
)rawliteral";