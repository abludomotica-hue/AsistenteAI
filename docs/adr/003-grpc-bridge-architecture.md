# ADR-003: Arquitectura Bridge Python (gRPC) vs Conexión Directa

**Fecha:** Agosto 2026
**Estado:** Aceptado

## Contexto
Los servicios de NVIDIA NIM (Parakeet ASR, Magpie TTS) utilizan gRPC como estándar de transporte. El ESP32-P4 no cuenta con la memoria, ni con la robustez criptográfica o stack de software para soportar gRPC sobre TLS sin desestabilizar la UI asíncrona y el motor de audio simultáneamente. 

## Decisión
Adoptar una arquitectura "Proxy" (Bridge). El ESP32 habla HTTP REST básico a la VM en la red local. El Bridge Server (`bridge_server.py`) asume la carga completa de autenticación con `nvcf.nvidia.com`, la negociación del protocolo binario gRPC y la lógica de estado del LLM (Nemotron).

## Consecuencias
- **Positivas:** Cero secretos en el hardware (protege API Keys), el ESP32 no sufre picos de consumo de CPU por cifrado TLS, y permite pre-procesamiento del LLM para control domótico.
- **Negativas:** Introduce un punto de fallo único local (la VM Debian de Proxmox) en lugar de que el dispositivo dependa exclusivamente del proveedor Cloud.
