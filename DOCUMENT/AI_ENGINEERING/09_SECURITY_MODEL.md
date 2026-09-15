# 09. SECURITY & CRYPTOGRAPHIC MODEL
## Threat Model, Secrets Management & Hardware Security

---

## 1. Secrets Management Policy
- **Zero Secrets in Git:** Ningún archivo que contenga API keys (`NVIDIA_API_KEY`, `LLM_API_KEY`, `BRIDGE_AUTH_TOKEN`) o contraseñas WiFi (`WIFI_PASS`) puede ser versionado en el repositorio.
- **Example Templates:** Se proporcionan plantillas públicas `main/config.h.example` y `bridge.env.example` protegidas por `.gitignore`.

---

## 2. Network & Gateway Security Model
- **Token Authentication:** El servidor Gateway en Debian valida la cabecera `X-Bridge-Token` cuando `BRIDGE_AUTH_TOKEN` está configurado en `.env`.
- **LAN Isolation:** El Gateway escucha en la subred privada local (`192.168.1.0/24`) y no expone puertos hacia internet público sin túneles cifrados (TLS/VPN).
- **TLS/HTTPS Transition (Fase 4):** Para producción final, se implementará TLS 1.3 entre el ESP32 y el Gateway con Certificate Pinning.

---

## 3. Firmware Security Roadmap (Production)
- **Secure Boot v2:** Firma criptográfica del binario con clave RSA-3072 / ECDSA.
- **Flash Encryption:** Cifrado transparente por hardware AES-XTS-256 de la memoria Flash SPI y PSRAM del ESP32-P4.
- **Anti-Rollback:** Control de versiones en partición `otadata` para impedir el flasheo de binarios antiguos vulnerables.
