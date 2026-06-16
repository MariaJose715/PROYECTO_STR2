# STR 2026 - Sistema de Control Ambiental Automatizado

Proyecto final de **Sistemas de Tiempo Real 2026** — Universidad Mariano Gálvez.

Sistema IoT embebido en **ESP32-C6** que automatiza la ventilación, iluminación
LED RGB y cortinas de una habitación en función de la temperatura ambiente.
Se opera de forma remota mediante un panel web responsivo embebido en el
propio ESP32. Soporta WiFi Station + AP, actualización OTA y sincronización
NTP.

---

## Tabla de contenidos

- [Arquitectura del sistema](#arquitectura-del-sistema)
- [Componentes de hardware](#componentes-de-hardware)
- [Diagrama de conexión](#diagrama-de-conexión)
- [Requerimientos de software](#requerimientos-de-software)
- [Estructura del proyecto](#estructura-del-proyecto)
- [Módulos del firmware](#módulos-del-firmware)
  - [GPIO](#gpio_handler)
  - [ADC / NTC](#adc_handler)
  - [PWM (LEDC)](#pwm_handler)
  - [LED RGB](#led_handler)
  - [UART](#uart_handler)
  - [WiFi](#wifi_handler)
  - [Servidor web](#web_server)
  - [OTA](#ota_handler)
- [Panel web](#panel-web)
- [API REST](#api-rest)
- [Sincronización de hora](#sincronización-de-hora)
- [Flujo de inicialización](#flujo-de-inicialización)
- [Cómo construir y flashear](#cómo-construir-y-flashear)
- [Uso](#uso)

---

## Arquitectura del sistema

El firmware corre sobre **FreeRTOS** en un **ESP32-C6** (RISC-V de 32 bits).
Usa el periférico **LEDC** para generar señales PWM que controlan el
ventilador (25 kHz, audible fuera del rango humano) y el servomotor de
cortinas (50 Hz, estándar para servos). La temperatura se mide con un
**NTC 10k (B=3950)** en divisor de voltaje, leído por el **ADC oneshot**
con calibración. La comunicación con el usuario se hace a través de un
**servidor HTTP** embebido que sirve un panel web responsivo y una API REST.

```
┌─────────────────────────────────────────────┐
│              ESP32-C6 (RISC-V)              │
│                                             │
│  ┌──────────┐   ┌──────────┐  ┌──────────┐ │
│  │  GPIO    │   │  LEDC    │  │  ADC     │ │
│  │  (pines) │   │  (PWM)   │  │  (NTC)   │ │
│  └────┬─────┘   └────┬─────┘  └────┬─────┘ │
│       │              │             │        │
│  ┌────▼─────┐   ┌────▼─────┐  ┌────▼─────┐ │
│  │LEDs RGB  │   │Ventilador│  │Sensor    │ │
│  │+ Alarma  │   │+ Servo   │  │Temp. NTC │ │
│  └──────────┘   └──────────┘  └──────────┘ │
│                                             │
│  ┌──────────┐   ┌──────────┐  ┌──────────┐ │
│  │  WiFi    │──▶│Serv.HTTP │──▶│Dashboard│ │
│  │AP+Station│   │(REST API)│   │ Web     │ │
│  └──────────┘   └──────────┘  └──────────┘ │
│                                             │
│  ┌──────────┐   ┌──────────┐               │
│  │  OTA     │   │  NTP     │               │
│  │(Update)  │   │(Hora)    │               │
│  └──────────┘   └──────────┘               │
└─────────────────────────────────────────────┘
```

---

## Componentes de hardware

| Componente            | Especificación                    | Pin ESP32-C6 |
|-----------------------|-----------------------------------|--------------|
| LED RGB               | Cátodo común, 3 canales PWM       | GPIO5 (R), GPIO6 (G), GPIO7 (B) |
| LED Alarma            | Rojo, encendido/apagado           | GPIO8        |
| Ventilador            | DC con entrada PWM (25 kHz)       | GPIO10       |
| Servomotor            | SG90 / similar (50 Hz, 0-180°)    | GPIO11       |
| Sensor temperatura    | NTC 10kΩ (B=3950) + resistor 10kΩ | GPIO4 (ADC1_CH4) |
| UART (debug)          | 115200 baud, 8N1                  | GPIO16 (TX), GPIO17 (RX) |
| Botón BOOT            | Pull-up interno, usado para reinicio | GPIO0     |

---

## Diagrama de conexión

```
3.3V ──── NTC ────┬─── GPIO4 (ADC) ──── 10kΩ ──── GND
                  │
                 GND (a través de R_fijo)

GPIO5  ──── 220Ω ──── LED_R (ánodo)
GPIO6  ──── 220Ω ──── LED_G (ánodo)
GPIO7  ──── 220Ω ──── LED_B (ánodo)
LEDs cátodo común ──── GND

GPIO8  ──── 220Ω ──── LED_Alarma ──── GND

GPIO10 ──── Driver/MOSFET ──── Ventilador DC
GPIO11 ──── Servomotor (línea de señal)
GPIO16 ──── TX de USB-UART
GPIO17 ──── RX de USB-UART
```

> **Nota**: El ventilador DC normalmente requiere un transistor
> (NPN, ej. 2N2222) o un MOSFET (ej. IRLZ44N) para ser manejado
> desde la salida PWM de 3.3V del ESP32.

---

## Requerimientos de software

- [ESP-IDF v5.5.4](https://github.com/espressif/esp-idf) — framework oficial
- Target: `esp32c6` (RISC-V)
- Python 3.11+ con venv
- CMake 3.16+, Ninja

### Instalación rápida (Windows)

```powershell
# El proyecto espera IDF en C:\esp\v5.5.4\.espressif\v5.5.4\esp-idf\
# y herramientas en C:\Esp32_IDF\Espressif\
$env:IDF_PATH="C:\esp\v5.5.4\.espressif\v5.5.4\esp-idf"
$env:IDF_TOOLS_PATH="C:\Esp32_IDF\Espressif"
$env:PATH="$env:IDF_TOOLS_PATH\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;"
$env:PATH+="$env:IDF_TOOLS_PATH\python_env\idf5.5_py3.13_env\Scripts;"
$env:PATH+="$env:IDF_TOOLS_PATH\tools\cmake\3.30.2\bin;"
$env:PATH+="$env:IDF_TOOLS_PATH\tools\ninja\1.12.1;$env:PATH"
$env:IDF_PYTHON_ENV_PATH="$env:IDF_TOOLS_PATH\python_env\idf5.5_py3.13_env"
python "$env:IDF_PATH\tools\idf.py" build
```

---

## Estructura del proyecto

```
PROYECTO_STR2/
├── CMakeLists.txt              # Proyecto ESP-IDF (target esp32c6)
├── sdkconfig                   # Configuración guardada (single_app_large)
├── main/
│   ├── CMakeLists.txt          # Compila 10 .c, incluye dashboard/
│   ├── main.c                  # Punto de entrada, bucle de control
│   ├── gpio_handler.c/h        # Inicialización de pines GPIO
│   ├── uart_handler.c/h        # Comunicación serial (debug)
│   ├── pwm_handler.c/h         # PWM con LEDC (ventilador + servo)
│   ├── adc_handler.c/h         # ADC + NTC (ecuación Beta)
│   ├── led_handler.c/h         # LED RGB (PWM) + LED alarma
│   ├── wifi_handler.c/h        # WiFi Station + AP, NVS
│   ├── web_server.c/h          # Servidor HTTP, API REST, 19 rutas
│   ├── ota_handler.c/h         # Actualización OTA vía HTTP
│   └── dashboard/
│       ├── index.html           # Panel web (fuente)
│       ├── style.css            # Estilos (fuente)
│       ├── app.js               # Lógica JS (fuente)
│       └── dashboard_content.c/h  # HTML/CSS/JS embebido (raw string literals C11)
└── build/                       # Build output (generado)
```

---

## Módulos del firmware

### gpio_handler

Inicializa todos los pines del sistema con `gpio_config_t` (patrón de los
ejemplos oficiales de ESP-IDF). Define las constantes de pines validadas
para **ESP32-C6** (no usa GPIOs inválidos como 32, 33, 34).

### adc_handler

Configura el ADC1 en modo **oneshot** con calibración (curve_fitting con
fallback a line_fitting). Lee el voltaje en el divisor resistivo formado
por el **NTC 10k (B=3950)** y una resistencia fija de **10kΩ**.

La temperatura se calcula con la **ecuación Beta**:

    1/T = 1/T₀ + (1/B) · ln(R_NTC / R₀)

donde T₀ = 298.15 K (25 °C), R₀ = 10 kΩ, B = 3950.

El circuito es:

    3.3V ─── NTC ─── ADC_GPIO4 ─── 10kΩ ─── GND

### pwm_handler

Usa el periférico **LEDC** (modo LOW_SPEED, requerido en ESP32-C6) para
generar dos señales PWM independientes:

| Dispositivo  | Timer | Canal | Frecuencia | Resolución | Duty 0% | Duty 100% |
|--------------|-------|-------|------------|------------|---------|-----------|
| Ventilador   | 0     | 0     | 25 kHz     | 8-bit      | 0       | 255       |
| Servo        | 1     | 1     | 50 Hz      | 14-bit     | 819     | 1638      |

El servo se mapea linealmente: 0% = 0° (duty 819), 100% = 180° (duty 1638).

### led_handler

- **LED RGB**: 3 canales PWM independientes con **LEDC** (timer 2, 1 kHz,
  8-bit). El brillo se combina con el color: `duty = (componente · brillo) / 100`.
  Antes se usaba GPIO binario (ON/OFF), se corrigió para brillo variable.
- **LED Alarma**: GPIO digital, parpadea a 1 Hz en tarea FreeRTOS separada
  cuando la temperatura excede el máximo.

### uart_handler

Comunicación serial a **115200 baud**. Proporciona `uart_send_msg()` con
formato printf para depuración del sistema. No bloquea el bucle principal.

### wifi_handler

Arranca en modo **AP + Station** simultáneo:

- **AP**: SSID por defecto `maria_esp`, contraseña `12345678`, máximo 4
  clientes. Las credenciales se guardan en NVS y se pueden cambiar desde
  el panel web.
- **Station**: Intenta conectarse a una red WiFi guardada en NVS con hasta
  5 reintentos. Reporta estado vía `wifi_esta_conectado()`.

### web_server

Servidor HTTP en el puerto 80 usando `esp_http_server`. Sirve:

1. **Páginas estáticas**: HTML, CSS, JS embebidos como raw string literals
   C11 en `dashboard_content.c` (3 archivos concatenados).
2. **API REST**: 19 rutas para consultar y controlar todos los parámetros
   del sistema.

Las rutas estáticas incluyen `Cache-Control: no-cache` para evitar caché
del navegador durante el desarrollo.

### ota_handler

Actualización Over-The-Air mediante `esp_https_ota`. Recibe una URL por
POST y descarga el firmware .bin en una tarea FreeRTOS separada. Soporta
rollback en caso de fallo.

---

## Panel web

El dashboard se accede desde cualquier navegador en la IP del ESP32
(192.168.4.1 en modo AP, o la IP asignada por el router en modo Station).

### Secciones

| Sección              | Descripción                                   |
|----------------------|-----------------------------------------------|
| Temperatura          | Lectura actual + input de temp deseada/máxima |
| Ventilador           | Modo auto (proporcional) o manual             |
| Cortinas             | Modo programado (horarios) o manual           |
| LED RGB              | Selector de color + brillo                    |
| Red WiFi             | Estado de conexión, IP, hora actual           |
| Actualización OTA    | URL del firmware .bin para actualizar          |

### Lógica de control automático

**Ventilador** (control proporcional):

- `T ≤ T_deseada` → 0%
- `T ≥ T_máxima` → 100%
- `T_deseada < T < T_máxima` → `((T - T_deseada) / (T_máxima - T_deseada)) × 100`

**Cortinas** (modo programado):

- Se definen hasta 8 horarios (hora, minuto, apertura en %)
- Cada 30 segundos el sistema compara la hora actual (NTP o local) contra
  los horarios activos y ejecuta el servo cuando hay coincidencia.

---

## API REST

Todas las rutas devuelven `application/json`.

### Consulta

| Método | Ruta                   | Descripción                                 |
|--------|------------------------|---------------------------------------------|
| GET    | `/`                    | Página HTML del dashboard                   |
| GET    | `/style.css`           | Estilos CSS                                 |
| GET    | `/app.js`              | JavaScript del dashboard                    |
| GET    | `/api/ping`            | `{"pong":true,"server":"STR2026"}`          |
| GET    | `/api/temp`            | Temperatura, ventilador, cortina, hora      |
| GET    | `/api/curtain/schedule`| Horarios programados de cortina             |
| GET    | `/api/ota/version`     | Versión actual del firmware                 |
| GET    | `/api/time`            | Hora del sistema (hora, min, seg, anio...)  |

### Control

| Método | Ruta                     | Body (JSON)                                    |
|--------|--------------------------|------------------------------------------------|
| POST   | `/api/fan/mode`          | `{"auto_mode": true/false}`                    |
| POST   | `/api/fan/config`        | `{"temp_deseada": 25.0, "temp_maxima": 35.0}` |
| POST   | `/api/fan/speed`         | `{"speed": 50}`                                |
| POST   | `/api/curtain/mode`      | `{"auto_mode": true/false}`                    |
| POST   | `/api/curtain/position`  | `{"position": 50}`                             |
| POST   | `/api/curtain/schedule`  | `{"schedules": [{"hora":8,"minuto":0,"apertura":50,"activo":true}]}` |
| POST   | `/api/rgb`               | `{"r":255,"g":255,"b":255,"brillo":50}`        |
| POST   | `/api/wifi/sta`          | `{"ssid":"...","password":"..."}`              |
| POST   | `/api/wifi/ap`           | `{"ssid":"...","password":"..."}`              |
| POST   | `/api/ota`               | `{"url":"http://..."}`                         |
| POST   | `/api/time/set`          | `{"hora":14,"min":30,"seg":0}`                 |

### Ejemplo de respuesta de `/api/temp`

```json
{
  "temperatura": 28.5,
  "temp_deseada": 25.0,
  "temp_maxima": 35.0,
  "fan_speed": 35,
  "fan_auto_mode": true,
  "curtain_pos": 50,
  "alarma": false,
  "wifi_ip": "192.168.1.100",
  "wifi_conectado": true,
  "hora": "14:30:15",
  "hora_sincronizada": true
}
```

---

## Sincronización de hora

El sistema soporta tres formas de obtener la hora:

1. **NTP** (automático): Al iniciar, si WiFi Station está conectado, el
   sistema consulta `pool.ntp.org` y `time.google.com`. Espera hasta 15
   segundos por una respuesta.
2. **Fallback hardcodeado**: Si no hay WiFi o NTP no responde, se usa
   `2026-06-10 08:00:00` (zona horaria UTC-6).
3. **Manual**: El usuario puede ajustar la hora vía `POST /api/time/set`
   para pruebas de horarios programados.

El flag `hora_sincronizada` en la API indica si NTP respondió exitosamente.

---

## Flujo de inicialización

```
app_main()
  ├── gpio_init_all()         ── Pines GPIO
  ├── uart_init()             ── Serial 115200
  ├── adc_temp_init()         ── ADC + NTC
  ├── pwm_fan_init()          ── PWM ventilador   (timer 0, 25 kHz)
  ├── pwm_servo_init()        ── PWM servo        (timer 1, 50 Hz)
  ├── led_rgb_init()          ── RGB LED          (timer 2, 1 kHz)
  ├── led_alarma_init()       ── LED alarma
  ├── ota_init()              ── OTA handler
  ├── wifi_init()             ── WiFi AP + Station
  ├── [esperar 10s WiFi]      ── Espera conexión Station
  ├── sincronizar_hora_ntp()  ── NTP (pool.ntp.org, 15s timeout)
  │   └── (fallback)          ── Hora hardcodeada si NTP falla
  ├── web_server_start()      ── HTTP server (puerto 80, 19 rutas)
  └── xTaskCreate(control)    ── Tarea FreeRTOS de control
       └── loop (c/2s):
            ├── leer temp
            ├── controlar ventilador (auto/manual)
            ├── verificar alarma
            ├── verificar horarios cortina (c/30s)
            └── vTaskDelay(2000ms)
```

---

## Cómo construir y flashear

### Requisitos

- ESP-IDF v5.5.4 instalado en `C:\esp\v5.5.4\.espressif\v5.5.4\esp-idf\`
- Toolchain riscv32-esp-elf
- CMake 3.30+, Ninja 1.12+
- Python 3.11+ (entorno virtual en `idf5.5_py3.13_env`)

### Compilar

```powershell
# Configurar entorno
$env:IDF_PATH="C:\esp\v5.5.4\.espressif\v5.5.4\esp-idf"
$env:IDF_TOOLS_PATH="C:\Esp32_IDF\Espressif"

$env:PATH = "$env:IDF_TOOLS_PATH\tools\riscv32-esp-elf\esp-14.2.0_20260121\riscv32-esp-elf\bin;" +
            "$env:IDF_TOOLS_PATH\python_env\idf5.5_py3.13_env\Scripts;" +
            "$env:IDF_TOOLS_PATH\tools\cmake\3.30.2\bin;" +
            "$env:IDF_TOOLS_PATH\tools\ninja\1.12.1;$env:PATH"
$env:IDF_PYTHON_ENV_PATH="$env:IDF_TOOLS_PATH\python_env\idf5.5_py3.13_env"

# Construir
python "$env:IDF_PATH\tools\idf.py" build
```

### Flashear

```powershell
python "$env:IDF_PATH\tools\idf.py" -p COM3 flash monitor
```

(Puerto COM ajustable según el sistema)

---

## Uso

1. **Alimentar el ESP32** (USB-C o fuente externa 5V).
2. **Conectarse al AP** `maria_esp` con contraseña `12345678`.
3. **Abrir navegador** en `http://192.168.4.1`.
4. El panel web muestra la temperatura, permite cambiar parámetros,
   configurar horarios y actualizar firmware.

Para conectar el ESP32 a una red WiFi existente (opcional):
- Ir a la sección **Red WiFi** en el dashboard
- Ingresar SSID y contraseña de la red
- El ESP32 se conecta y se puede acceder desde la IP asignada por el router
- El AP `maria_esp` sigue disponible como respaldo

---

## Partición

El proyecto usa `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE` porque el binario
(~1.12 MB) excede el tamaño de la partición factory predeterminada (1 MB).

---

## Historial de correcciones

| Problema | Solución |
|----------|----------|
| LED RGB sin variación de brillo | Se reemplazó GPIO binario (threshold 50) por PWM LEDC con 3 canales independientes |
| Ventilador no proporcional | Se corrigieron constantes NTC (R_FIJO=10k, R0=10k) y se agregó protección NaN en logf() |
| Inputs de temperatura se reiniciaban cada 3s | Se agregó verificación de `document.activeElement` en JS |
| Horario de cortinas no ejecutaba servo | Se agregó sincronización NTP + endpoint `/api/time/set` para pruebas |
| Dashboard embebido no se servía | Se migró de `EMBED_TXTFILES` a raw string literals C11 |
| Error JS (ternario incompleto) | Se corrigió `'Auto: '` → `'Auto: ':'` |
| Buffer overflow en schedule GET | Se aumentó buffer a 2048 bytes + safety check |

---

## Licencia

Proyecto académico — Sistemas de Tiempo Real, Universidad Mariano Gálvez, 2026.
