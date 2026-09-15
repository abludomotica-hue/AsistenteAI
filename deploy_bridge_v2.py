"""
Deploy Bridge Server v2 to Debian VM via SSH
=============================================
Este script automatiza la actualización del bridge_server.py en la VM Debian.

Uso:
  $env:DEBIAN_PASS = "tu_password_ssh"   # PowerShell
  python deploy_bridge_v2.py

Requisitos:
  pip install paramiko
  Archivo `bridge.env` en esta carpeta (copia bridge.env.example y
  rellena los valores reales). `bridge.env` está fuera de git.

Variables de entorno opcionales:
  DEBIAN_HOST (default 192.168.1.58), DEBIAN_USER (default ablutech),
  DEBIAN_PASS (obligatoria, sin default)
"""

import os
import sys
import time

try:
    import paramiko
except ImportError:
    print("Error: Necesitas instalar paramiko")
    print("  pip install paramiko")
    sys.exit(1)

# ==========================================
# CONFIGURACIÓN DE CONEXIÓN SSH
# ==========================================
HOSTNAME = os.getenv("DEBIAN_HOST", "192.168.1.58")
USERNAME = os.getenv("DEBIAN_USER", "ablutech")
PASSWORD = os.getenv("DEBIAN_PASS", "")

REMOTE_DIR = "/home/ablutech/riva-bridge"
SERVICE_NAME = "riva-bridge"

# ==========================================
# CONTENIDO DEL BRIDGE SERVER v2
# ==========================================
# Leer el archivo bridge_server.py del directorio actual
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BRIDGE_FILE = os.path.join(SCRIPT_DIR, "bridge_server.py")

if not os.path.exists(BRIDGE_FILE):
    print(f"Error: No se encontró {BRIDGE_FILE}")
    sys.exit(1)

with open(BRIDGE_FILE, 'r', encoding='utf-8') as f:
    server_code = f.read()

# ==========================================
# CONTENIDO DEL .env (desde bridge.env local, fuera de git)
# ==========================================
# Las API Keys y Function IDs reales viven en `bridge.env`
# (gitignored). Plantilla de referencia: bridge.env.example
ENV_FILE = os.path.join(SCRIPT_DIR, "bridge.env")

# ==========================================
# CONTENIDO DE REQUIREMENTS
# ==========================================
REQ_FILE = os.path.join(SCRIPT_DIR, "requirements.txt")
if not os.path.exists(REQ_FILE):
    print(f"Error: No se encontró {REQ_FILE}")
    sys.exit(1)

with open(REQ_FILE, 'r', encoding='utf-8') as f:
    req_content = f.read()


if not os.path.exists(ENV_FILE):
    print(f"Error: No se encontró {ENV_FILE}")
    print("Copia bridge.env.example a bridge.env y rellena los valores reales.")
    sys.exit(1)

with open(ENV_FILE, 'r', encoding='utf-8') as f:
    env_content = f.read()

# ==========================================
# CONTENIDO DEL SERVICIO SYSTEMD
# ==========================================
service_content = """[Unit]
Description=NVIDIA Riva Bridge Server v2 (Magpie TTS)
After=network.target

[Service]
User=ablutech
WorkingDirectory=/home/ablutech/riva-bridge
EnvironmentFile=/home/ablutech/riva-bridge/.env
ExecStart=/home/ablutech/riva-bridge/venv/bin/python /home/ablutech/riva-bridge/bridge_server.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
"""

# ==========================================
# FUNCIONES DE EJECUCIÓN REMOTA
# ==========================================
def execute(ssh, command, description=""):
    """Ejecuta un comando SSH y muestra el resultado."""
    if description:
        print(f"  → {description}")
    stdin, stdout, stderr = ssh.exec_command(command)
    exit_status = stdout.channel.recv_exit_status()
    out = stdout.read().decode('utf-8').strip()
    err = stderr.read().decode('utf-8').strip()
    if out:
        for line in out.split('\n'):
            print(f"    {line}")
    if err and exit_status != 0:
        print(f"    ⚠ {err}")
    return exit_status, out


def main():
    if not PASSWORD:
        print("Error: Falta la variable de entorno DEBIAN_PASS (password SSH).")
        print("  PowerShell:  $env:DEBIAN_PASS = 'tu_password'")
        print("  Bash:        export DEBIAN_PASS='tu_password'")
        sys.exit(1)

    print("=" * 60)
    print("  Deploy Bridge Server v2 → Debian VM")
    print(f"  Host: {HOSTNAME}")
    print(f"  User: {USERNAME}")
    print(f"  Dir:  {REMOTE_DIR}")
    print("=" * 60)

    try:
        # 1. Conectar por SSH
        print("\n[1/6] Conectando a Debian vía SSH...")
        ssh = paramiko.SSHClient()
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        ssh.connect(HOSTNAME, username=USERNAME, password=PASSWORD, timeout=10)
        print("  ✅ Conectado")

        # 2. Crear directorio si no existe
        print("\n[2/6] Preparando directorio remoto...")
        execute(ssh, f"mkdir -p {REMOTE_DIR}", "Creando directorio")

        # 3. Subir archivos
        print("\n[3/6] Subiendo archivos...")
        sftp = ssh.open_sftp()

        # bridge_server.py
        with sftp.file(f'{REMOTE_DIR}/bridge_server.py', 'w') as f:
            f.write(server_code)
        print("  ✅ bridge_server.py actualizado")

        # .env (solo si no existe o si el usuario quiere sobreescribir)
        env_exists = False
        try:
            sftp.stat(f'{REMOTE_DIR}/.env')
            env_exists = True
        except FileNotFoundError:
            pass

        if env_exists:
            # Hacer backup del .env existente
            execute(ssh, f"cp {REMOTE_DIR}/.env {REMOTE_DIR}/.env.backup",
                    "Backup de .env existente → .env.backup")
            # Escribir nuevo .env
            with sftp.file(f'{REMOTE_DIR}/.env', 'w') as f:
                f.write(env_content)
            print("  ✅ .env actualizado (backup guardado como .env.backup)")
        else:
            with sftp.file(f'{REMOTE_DIR}/.env', 'w') as f:
                f.write(env_content)
            print("  ✅ .env creado")
            
        # requirements.txt
        with sftp.file(f'{REMOTE_DIR}/requirements.txt', 'w') as f:
            f.write(req_content)
        print("  ✅ requirements.txt actualizado")

        # systemd service
        with sftp.file(f'{REMOTE_DIR}/riva-bridge.service', 'w') as f:
            f.write(service_content)
        print("  ✅ riva-bridge.service actualizado")

        sftp.close()

        # 4. Instalar/actualizar dependencias
        print("\n[4/6] Verificando dependencias de Python...")

        # Verificar si el venv existe
        status, _ = execute(ssh, f"test -d {REMOTE_DIR}/venv && echo 'exists'")
        if "exists" not in _:
            print("  Creando entorno virtual...")
            execute(ssh, f"python3 -m venv {REMOTE_DIR}/venv",
                    "python3 -m venv")

        execute(ssh, f"{REMOTE_DIR}/venv/bin/pip install -q --upgrade -r {REMOTE_DIR}/requirements.txt",
                "Instalando dependencias desde requirements.txt...")
        print("  ✅ Dependencias OK")

        # 5. Configurar y reiniciar systemd
        print("\n[5/6] Configurando servicio systemd...")
        execute(ssh, f"echo '{PASSWORD}' | sudo -S cp {REMOTE_DIR}/riva-bridge.service /etc/systemd/system/",
                "Copiando servicio")
        execute(ssh, f"echo '{PASSWORD}' | sudo -S systemctl daemon-reload",
                "daemon-reload")
        execute(ssh, f"echo '{PASSWORD}' | sudo -S systemctl enable {SERVICE_NAME}",
                "Habilitando servicio")
        execute(ssh, f"echo '{PASSWORD}' | sudo -S systemctl restart {SERVICE_NAME}",
                "Reiniciando servicio")
        print("  ✅ Servicio reiniciado")

        # 6. Verificar estado
        print("\n[6/6] Verificando estado...")
        time.sleep(2)  # Esperar a que arranque
        execute(ssh, f"echo '{PASSWORD}' | sudo -S systemctl status {SERVICE_NAME} --no-pager -l",
                "Estado del servicio")

        # Intentar el health check
        time.sleep(1)
        status, health_output = execute(ssh, "curl -s http://localhost:5000/health 2>/dev/null",
                                        "Health check")
        if health_output:
            print(f"\n  Health: {health_output}")

        ssh.close()

        print("\n" + "=" * 60)
        print("  ✅ DESPLIEGUE COMPLETADO")
        print("=" * 60)
        print(f"\n⚠️  ACCIÓN REQUERIDA:")
        print(f"   Si TTS_FUNCTION_ID dice 'REEMPLAZAR_CON_TU_FUNCTION_ID',")
        print(f"   edita el archivo .env en Debian:")
        print(f"     ssh {USERNAME}@{HOSTNAME}")
        print(f"     nano {REMOTE_DIR}/.env")
        print(f"   Luego reinicia: sudo systemctl restart {SERVICE_NAME}")
        print(f"\n   Test manual:")
        print(f"     curl http://{HOSTNAME}:5000/health")

    except paramiko.ssh_exception.AuthenticationException:
        print("  ❌ Error de autenticación SSH. Verifica usuario/contraseña.")
    except paramiko.ssh_exception.NoValidConnectionsError:
        print(f"  ❌ No se puede conectar a {HOSTNAME}. ¿Está encendida la VM?")
    except Exception as e:
        print(f"  ❌ Error: {e}")
        import traceback
        traceback.print_exc()


if __name__ == '__main__':
    main()
