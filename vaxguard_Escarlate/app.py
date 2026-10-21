from flask import Flask, request, jsonify
import sqlite3
import datetime

app = Flask(__name__)
DB_PATH = "vaxguard.db"

# Cria a tabela se não existir
def init_db():
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS leituras (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT,
            seq INTEGER,
            ts INTEGER,
            temp REAL,
            umid REAL,
            porta INTEGER,
            luz INTEGER,
            estado INTEGER,
            estado_anterior INTEGER,
            recebido_em TEXT
        )
    """)
    conn.commit()
    conn.close()

# Endpoint que o Arduino vai chamar
@app.route('/api/telemetry', methods=['POST'])
def receber_telemetria():
    dados = request.get_json()
    if not dados:
        return jsonify({"erro": "JSON inválido"}), 400

    # Validação simples dos campos obrigatórios
    obrigatorios = ['device_id', 'temp', 'umid', 'porta', 'estado']
    for campo in obrigatorios:
        if campo not in dados:
            return jsonify({"erro": f"Campo faltando: {campo}"}), 400

    agora = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    try:
        conn = sqlite3.connect(DB_PATH)
        conn.execute("""
            INSERT INTO leituras (device_id, seq, ts, temp, umid, porta, luz, estado, estado_anterior, recebido_em)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            dados.get('device_id'),
            dados.get('seq'),
            dados.get('ts'),
            dados.get('temp'),
            dados.get('umid'),
            1 if dados.get('porta') else 0,
            dados.get('luz', 0),
            dados.get('estado'),
            dados.get('estado_anterior'),
            agora
        ))
        conn.commit()
        conn.close()
        print(f"[{agora}] Recebido de {dados['device_id']}: {dados['temp']}°C")
        return jsonify({"status": "ok"}), 200
    except Exception as e:
        print(f"Erro ao salvar: {e}")
        return jsonify({"erro": str(e)}), 500

# Endpoint para o dashboard consultar os dados depois
@app.route('/api/leituras', methods=['GET'])
def listar_leituras():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.execute("SELECT * FROM leituras ORDER BY id DESC LIMIT 50")
    colunas = [d[0] for d in cursor.description]
    linhas = [dict(zip(colunas, row)) for row in cursor.fetchall()]
    conn.close()
    return jsonify(linhas)

if __name__ == '__main__':
    init_db()
    print("Servidor VaxGuard rodando em http://0.0.0.0:5000")
    app.run(host='0.0.0.0', port=5000, debug=True)