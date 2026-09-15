"""
Deploy Bridge Server v2 to Debian VM via SSH (Hardened Edition)
===============================================================
Automatiza el despliegue seguro de bridge_server.py y sus dependencias en la VM Debian.

Uso:
  Opción A (Llave SSH recomendada):
    python deploy_bridge_v2.py  (detecta automáticamente ~/.ssh/id_ed25519 o id_rsa)

  Opción B (Contraseña SSH):
    $env:DEBIAN_PASS = "tu_password_ssh"   # PowerShell
    python deploy_bridge_v2.py

Requisitos:
  pip install paramiko
  Archivo `bridge.env` en esta carpeta (copia bridge.env.example y rellena los valores reales).

Variables de entorno opcionales:
  DEBIAN_HOST (default: 192.168.1.58)
  DEBIAN_USER (default: ablutech)
  DEBIAN_PASS (password SSH y sudo, opcional si se usa llave y NOPASSWD)
  DEBIAN_KEY  (ruta a llave privada SSH personalizada)
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
CUSTOM_KEY = os.getenv("DEBIAN_KEY", "")

REMOTE_DIR = "/home/ablutech/riva-bridge"
SERVICE_NAME = "riva-bridge"

# ==========================================
# LECTURA DE ARCHIVOS LOCALES
# ==========================================
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BRIDGE_FILE = os.path.join(SCRIPT_DIR, "bridge_server.py")
ENV_FILE = os.path.join(SCRIPT_DIR, "bridge.env")
REQ_FILE = os.path.join(SCRIPT_DIR, "requirements.txt")

if not os.path.exists(BRIDGE_FILE):
    print(f"❌ Error: No se encontró {BRIDGE_FILE}")
    sys.exit(1)

with open(BRIDGE_FILE, 'r', encoding='utf-8') as f:
    server_code = f.read()

if not os.path.exists(REQ_FILE):
    print(f"❌ Error: No se encontró {REQ_FILE}")
    sys.exit(1)

with open(REQ_FILE, 'r', encoding='utf-8') as f:
    req_content = f.read()

env_content = None
if os.path.exists(ENV_FILE):
    with open(ENV_FILE, 'r', encoding='utf-8') as f:
        env_content = f.read()
else:
    print("⚠️  Aviso: No se encontró bridge.env. Si el servidor remoto ya tiene .env configurado, se preservará.")

# ==========================================
# DEFINICIÓN DEL SERVICIO SYSTEMD
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
# FUNCIONES DE EJECUCIÓN REMOTA SEGURA
# ==========================================
def execute(ssh, command, description=""):
    """Ejecuta un comando SSH estándar."""
    if description:
        print(f"  → {description}")
    stdin, stdout, stderr = ssh.exec_command(command)
    exit_status = stdout.channel.recv_exit_status()
    out = stdout.read().decode('utf-8', errors='replace').strip()
    err = stderr.read().decode('utf-8', errors='replace').strip()
    if out:
        for line in out.split('\n'):
            print(f"    {line}")
    if err and exit_status != 0:
        print(f"    ⚠ {err}")
    return exit_status, out


def execute_sudo(ssh, command, sudo_pass="", description=""):
    """
    Ejecuta un comando con sudo de forma segura.
    Alimenta la contraseña vía stdin interactivo de Paramiko para evitar
    exponerla en la línea de comandos /proc/*/cmdline.
    """
    if description:
        print(f"  → [sudo] {description}")
    stdin, stdout, stderr = ssh.exec_command(f"sudo -S -p '' {command}")
    
    if sudo_pass:
        stdin.write(f"{sudo_pass}\n")
        stdin.flush()
        
    exit_status = stdout.channel.recv_exit_status()
    out = stdout.read().decode('utf-8', errors='replace').strip()
    err = stderr.read().decode('utf-8', errors='replace').strip()
    if out:
        for line in out.split('\n'):
            print(f"    {line}")
    if err and exit_status != 0:
        print(f"    ⚠ {err}")
    return exit_status, out


def find_ssh_key():
    """Identifica si existe una llave privada SSH utilizable."""
    if CUSTOM_KEY and os.path.exists(CUSTOM_KEY):
        return CUSTOM_KEY
    candidates = [
        os.path.expanduser("~/.ssh/id_ed25519"),
        os.path.expanduser("~/.ssh/id_rsa")
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return None


def main():
    key_file = find_ssh_key()

    if not PASSWORD and not key_file:
        print("❌ Error: No se encontró método de autenticación SSH.")
        print("  Configura una llave en ~/.ssh/id_ed25519 (o id_rsa)")
        print("  O define la contraseña en la variable de entorno:")
        print("    PowerShell:  $env:DEBIAN_PASS = 'tu_password'")
        print("    Bash:        export DEBIAN_PASS='tu_password'")
        sys.exit(1)

    print("=" * 60)
    print("  Deploy Bridge Server v2 (Hardened) → Debian VM")
    print(f"  Host: {HOSTNAME}")
    print(f"  User: {USERNAME}")
    print(f"  Auth: {'Llave SSH (' + os.path.basename(key_file) + ')' if key_file else 'Contraseña'}")
    print(f"  Dir:  {REMOTE_DIR}")
    print("=" * 60)

    ssh = paramiko.SSHClient()
    try:
        ssh.load_system_host_keys()
    except Exception:
        pass
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

    connected = False
    try:
        print("\n[1/6] Conectando a Debian vía SSH...")
        connect_kwargs = {
            "hostname": HOSTNAME,
            "username": USERNAME,
            "timeout": 10
        }
        if key_file:
            connect_kwargs["key_filename"] = key_file
            if PASSWORD:
                connect_kwargs["password"] = PASSWORD  # Soporta llaves con passphrase o fallback
        else:
            connect_kwargs["password"] = PASSWORD

        ssh.connect(**connect_kwargs)
        connected = True
        print("  ✅ Conexión establecida exitosamente.")

        # 2. Preparar directorio remoto
        print("\n[2/6] Preparando directorio remoto...")
        execute(ssh, f"mkdir -p {REMOTE_DIR}", "Verificando estructura de directorios")

        # 3. Subir archivos actualizados
        print("\n[3/6] Sincronizando archivos con SFTP...")
        sftp = ssh.open_sftp()

        # bridge_server.py
        with sftp.file(f'{REMOTE_DIR}/bridge_server.py', 'w') as f:
            f.write(server_code)
        print("  ✅ bridge_server.py actualizado")

        # requirements.txt
        with sftp.file(f'{REMOTE_DIR}/requirements.txt', 'w') as f:
            f.write(req_content)
        print("  ✅ requirements.txt actualizado (versiones reforzadas)")

        # .env
        if env_content is not None:
            env_exists = False
            try:
                sftp.stat(f'{REMOTE_DIR}/.env')
                env_exists = True
            except FileNotFoundError:
                pass

            if env_exists:
                execute(ssh, f"cp {REMOTE_DIR}/.env {REMOTE_DIR}/.env.backup", "Backup de .env existente")
            with sftp.file(f'{REMOTE_DIR}/.env', 'w') as f:
                f.write(env_content)
            print("  ✅ .env sincronizado")

        # systemd service
        with sftp.file(f'{REMOTE_DIR}/riva-bridge.service', 'w') as f:
            f.write(service_content)
        print("  ✅ riva-bridge.service actualizado")

        sftp.close()

        # 4. Actualizar dependencias en entorno virtual
        print("\n[4/6] Actualizando dependencias de Python...")
        status, _ = execute(ssh, f"test -d {REMOTE_DIR}/venv && echo 'exists'")
        if "exists" not in _:
            execute(ssh, f"python3 -m venv {REMOTE_DIR}/venv", "Creando entorno virtual venv")

        execute(ssh, f"{REMOTE_DIR}/venv/bin/pip install -q --upgrade pip", "Actualizando pip en venv")
        status, _ = execute(ssh, f"{REMOTE_DIR}/venv/bin/pip install -q --upgrade -r {REMOTE_DIR}/requirements.txt",
                            "Instalando dependencias desde requirements.txt...")
        if status == 0:
            print("  ✅ Dependencias actualizadas y verificadas")
        else:
            print("  ⚠ Hubo advertencias o errores durante la instalación de paquetes.")

        # 5. Configurar y reiniciar systemd
        print("\n[5/6] Configurando y reiniciando servicio systemd...")
        execute_sudo(ssh, f"cp {REMOTE_DIR}/riva-bridge.service /etc/systemd/system/", PASSWORD, "Copiando unidad de servicio")
        execute_sudo(ssh, "systemctl daemon-reload", PASSWORD, "systemctl daemon-reload")
        execute_sudo(ssh, f"systemctl enable {SERVICE_NAME}", PASSWORD, f"Habilitando {SERVICE_NAME}")
        execute_sudo(ssh, f"systemctl restart {SERVICE_NAME}", PASSWORD, f"Reiniciando {SERVICE_NAME}")
        print("  ✅ Servicio reiniciado con éxito")

        # 6. Verificación de estado y salud
        print("\n[6/6] Verificando estado del servicio...")
        time.sleep(2)
        execute_sudo(ssh, f"systemctl status {SERVICE_NAME} --no-pager -l", PASSWORD, "Consultando estado systemd")

        time.sleep(1)
        status, health_output = execute(ssh, "curl -s http://localhost:5000/health 2>/dev/null", "Ejecutando health check local")
        if health_output:
            print(f"\n  📡 Diagnóstico de Salud (Health): {health_output}")

        print("\n" + "=" * 60)
        print("  ✅ DESPLIEGUE SEGURO COMPLETADO CON ÉXITO")
        print("=" * 60)

    except paramiko.ssh_exception.AuthenticationException:
        print("  ❌ Error de autenticación SSH. Verifica las credenciales o la llave privada.")
    except paramiko.ssh_exception.NoValidConnectionsError:
        print(f"  ❌ No se puede conectar a {HOSTNAME}:22. Verifica que la VM en Proxmox esté encendida.")
    except Exception as e:
        print(f"  ❌ Error inesperado: {e}")
        import traceback
        traceback.print_exc()
    finally:
        if connected:
            ssh.close()


if __name__ == '__main__':
    main()
