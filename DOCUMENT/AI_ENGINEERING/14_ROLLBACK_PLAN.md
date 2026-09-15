# 14. ROLLBACK & DISASTER RECOVERY PLAN
## Emergency Recovery Protocols

---

## 1. Firmware Recovery Procedures

### Scenario A: Bootloop / Hard Fault Panic
1. **Paso 1:** Conectar el ESP32 por cable USB Type-C.
2. **Paso 2:** Poner el dispositivo en modo Download manteniendo pulsado el botón **BOOT (GPIO 35)** mientras se pulsa y suelta el botón **RESET**.
3. **Paso 3:** Borrar la memoria flash para eliminar configuraciones NVS corruptas:
   ```powershell
   idf.py -p COM3 erase-flash
   ```
4. **Paso 4:** Reflashear el último binario certificado conocido:
   ```powershell
   idf.py -p COM3 flash
   ```

---

## 2. Gateway Service Recovery (Debian VM)

### Scenario B: Waitress WSGI Server Hang / Python Crash
1. **Paso 1:** Conectarse por SSH a la VM Debian (`192.168.1.58`).
2. **Paso 2:** Reiniciar el servicio de systemd:
   ```bash
   sudo systemctl restart riva-bridge
   ```
3. **Paso 3:** Verificar el estado del servicio:
   ```bash
   sudo systemctl status riva-bridge
   ```
4. **Paso 4:** Si el código Python se corrompió, restaurar la versión anterior con Git:
   ```bash
   git checkout HEAD~1 bridge_server.py
   sudo systemctl restart riva-bridge
   ```
