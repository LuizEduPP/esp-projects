# Hardware — Edge Pair (UART S3-CAM ↔ CYD)

Fiação entre **GOOUUU ESP32-S3-CAM** e **ESP32-2432S028 (CYD)** para o bus interno UART do [edge-pair](README.md).

Ambas as placas usam lógica **3,3 V** — **não** use conversor de nível TTL 5 V.

---

## Resumo da ligação

| Fio | GOOUUU S3-CAM | CYD (2432S028) |
|-----|---------------|----------------|
| **TX → RX** | **TX0** GPIO **43** | **P3 GPIO 35** (RX) |
| **RX ← TX** | **RX0** GPIO **44** | **P1 TX** (GPIO 1) |
| **5 V** | 5 V (header) | P1 **VIN** |
| **GND** | GND | P1 **GND** |

```
   [ USB ou fonte 5 V — UM ÚNICO PONTO ]
                    │
                    ▼ 5V + GND (barra comum)
   GOOUUU ESP32-S3-CAM                    ESP32-2432S028 (CYD)
   ┌──────────────────┐                  ┌──────────────────┐
   │ 5 V  ◄───────────┼──────────────────┼──► 5 V  (P1 VIN) │
   │ GND ─────────────┼──────────────────┼──── GND  (P1)    │
   │ TX0 / G43 (TX) ──┼──────────────────┼──► P3 GPIO35 (RX)│
   │ RX0 / G44 (RX) ◄─┼──────────────────┼──── P1 TX (G1)   │
   └──────────────────┘                  └──────────────────┘
        ▲ USB (opção A)                        (sem USB na operação)
```

> **P1 RX (GPIO 3) sem fio** — compartilhado com CH340; ver [modo híbrido](#p1p3-híbrido-uart-peer--serial).

**Regra UART:** TX de um lado sempre no **RX** do outro (cruzado).

---

## Pinagem escolhida

### GOOUUU ESP32-S3-CAM (edge-s3)

| Função | GPIO | Silk GOOUUU |
|--------|------|-------------|
| `PEER_TX` | **43** | **TX0** |
| `PEER_RX` | **44** | **RX0** |

Mesmos pinos do header usados como I2C no [mini-games](../goouuu-esp32-s3-cam/mini-games/HARDWARE.md) — no edge-pair são **UART**, sem OLED.

#### GPIOs proibidos na S3 (não usar para UART)

| GPIO | Motivo |
|------|--------|
| 4–18, 8–13, 15, 16 | Câmera OV2640 (interno) |
| 35–42, 45 | microSD / PSRAM |
| 19, 20 | USB CDC nativo (flash/monitor) |
| 0, 3, 46 | Strapping |
| 48 | LED RGB WS2812 onboard |

Referência: [mini-games/HARDWARE.md](../goouuu-esp32-s3-cam/mini-games/HARDWARE.md).

### CYD ESP32-2432S028 (edge-cyd)

Duas formas de ligar UART à S3. Na **ESP32-2432S028** (silk laranja):

| Modo | Conector | TX / RX (GPIO) | Quando usar |
|------|----------|----------------|-------------|
| **P1+P3 híbrido** ✅ uart-peer | P1 TX + P3 RX | TX **1** (P1), RX **35** (P3) | Mantém `Serial` / UART0 — **validado** no uart-peer |
| **P3 completo** ✅ edge-pair | JST 4 pinos (GPIO livres) | TX **22**, RX **35** | Link UART com S3 via `Serial2` — sem conflito com CH340 |
| **P1 (TX/RX)** ⚠️ evitar | JST 4 pinos na borda | TX **1**, RX **3** | Compartilha UART0 com USB — RX falha mesmo sem USB |

#### Conector P1 — TX/RX (+ VIN)

Conector JST **4 pinos** silk **TX/RX** na CYD (ligado ao UART0 / CH340 interno):

```
┌─────────┐
│   VIN   │  ← 5 V (trilho + MB102) — NÃO 3,3 V
│   TX    │  ← GPIO 1  → liga no RX da S3
│   RX    │  ← GPIO 3  (sem fio — CH340)
│   GND   │  ← trilho − MB102
└─────────┘
```

#### P1+P3 híbrido (uart-peer / Serial)

Mantém `Serial` (UART0) com RX remapeado para GPIO 35 — **não** usa P1 RX (GPIO 3).

```
MB102 +5V dir. ──────► P1 VIN
MB102 GND    ──────► P1 GND

GOOUUU TX0 / GPIO 43 (TX) ──► P3 GPIO 35 (RX)
GOOUUU RX0 / GPIO 44 (RX) ◄── P1 GPIO 1 (TX)
GOOUUU GND         ─── P1 GND (mesmo trilho −)
```

| Conector | Pino | GOOUUU | MB102 |
|----------|------|--------|-------|
| **P1 VIN** | 5 V | — | Trilho **+ 5 V** |
| **P1 GND** | GND | **GND** | Trilho **−** |
| **P1 TX** | GPIO 1 | GPIO **44** (RX) | — |
| **P3 GPIO 35** | RX | GPIO **43** (TX) | — |

**Firmware CYD (uart-peer — `Serial` remapeado):**

```cpp
#define PIN_PEER_TX  1   // P1 TX
#define PIN_PEER_RX  35  // P3 RX
Serial.begin(PEER_UART_BAUD, SERIAL_8N1, PIN_PEER_RX, PIN_PEER_TX);
```

**Firmware S3 (sem mudança):**

```cpp
HardwareSerial PeerSerial(1);
PeerSerial.begin(PEER_UART_BAUD, SERIAL_8N1, PIN_PEER_RX, PIN_PEER_TX);  // RX=44, TX=43
```

> **Por que não P1 RX (GPIO 3):** UART0 compartilhada com CH340 — TX (GPIO 1) funciona, RX (GPIO 3) não recebe `PAIR:HELLO` mesmo sem USB. Remapear RX para **GPIO 35** resolve sem trocar para `Serial2`.

#### P3 completo (edge-pair / Serial2)

**Ligação (MB102 + P1 alimentação + P3 UART):**

```
MB102 +5V dir. ──────► P1 VIN
MB102 GND    ──────► P1 GND

GOOUUU TX0 / GPIO 43 (TX) ──► P3 GPIO 35 (RX)
GOOUUU RX0 / GPIO 44 (RX) ◄── P3 GPIO 22 (TX)
GOOUUU GND         ─── P1 GND (mesmo trilho −)
```

| Conector | Pino | GOOUUU | MB102 |
|----------|------|--------|-------|
| **P1 VIN** | 5 V | — | Trilho **+ 5 V** |
| **P1 GND** | GND | **GND** | Trilho **−** |
| **P3 GPIO 35** | RX | GPIO **43** (TX) | — |
| **P3 GPIO 22** | TX | GPIO **44** (RX) | — |

**Firmware CYD (P3 = UART2):**

```cpp
// P3: TX=GPIO22, RX=GPIO35
Serial2.begin(PEER_UART_BAUD, SERIAL_8N1, 35, 22);
```

**Firmware S3 (sem mudança):**

```cpp
HardwareSerial PeerSerial(1);
PeerSerial.begin(PEER_UART_BAUD, SERIAL_8N1, 2, 1);  // RX=2, TX=1
```

> **Por que não P1 RX (GPIO 3):** GPIO 1/3 são UART0 compartilhada com o CH340. Use **P3 GPIO 35** para RX (híbrido com `Serial`) ou **P3 (22/35)** com `Serial2`; P1 fica VIN/GND + TX (GPIO 1) no modo híbrido.

**3,3 V do MB102:** **não** ligue no **VIN** — VIN exige **5 V**.

---

#### Conector P3 (alternativa — UART paralela ao USB)

Silk típico (4 pinos JST 1,25 mm):

```
GND · GPIO35 · GPIO22 · GPIO21
```

Use **GND**, **35** (RX) e **22** (TX). GPIO 21 = backlight — não use para UART.

```cpp
Serial2.begin(PEER_UART_BAUD, SERIAL_8N1, 35, 22);
```

Permite USB na CYD para debug **enquanto** fala com a S3.

#### Conector CN1

```
GND · GPIO22 · GPIO27 · 3V3
```

TX alternativo na **27**: `Serial2.begin(PEER_UART_BAUD, SERIAL_8N1, 35, 27);`

#### GPIOs a evitar na CYD

| GPIO | Motivo |
|------|--------|
| 4, 16, 17 | LED RGB onboard |
| 2, 12–15, 18–19, 23, 25, 32–33, 39 | TFT, touch, SD |
| 21 | Backlight TFT |
| 26 | Áudio |
| 0 | BOOT |

#### Montagem via GPIO do LED RGB *(só se não usar P1)*

Se **não** tiver cabo no P1 e puxar fio no LED RGB soldado:

| Cor | GPIO |
|-----|------|
| Verde | **16** |
| Azul | **17** |

`Serial2.begin(PEER_UART_BAUD, SERIAL_8N1, 16, 17);` — LED pisca durante UART. **P1 é preferível** (VIN + TX + RX + GND juntos).

---

## Alimentação única

O par usa **um só ponto de energia** — um cabo USB na tomada **ou** uma fonte bench. As duas placas compartilham o mesmo **5 V** e **GND** (barra comum ou jumpers).

### Consumo estimado

| Placa | Corrente típica | Picos |
|-------|-----------------|-------|
| GOOUUU S3-CAM (Wi-Fi + câmera) | 250–400 mA | ~500 mA |
| CYD (backlight + TFT) | 200–350 mA | ~500 mA |
| **Par** | **450–750 mA** | **≤ 1,2 A** |

Use fonte **5 V com ≥ 2 A** de margem (carregador USB, buck, bench).

---

### Opção A — Um USB alimenta as duas (recomendado em dev)

Um carregador **5 V / 2 A** (ou better) entra **só na GOOUUU**; a CYD recebe 5 V derivado pelo header.

```
Tomada → [Carregador 5V 2A] → USB-C → GOOUUU S3-CAM
                                         │
                         5 V (header) ───┼──► P1 VIN (CYD)
                         GND (header) ───┼──► GND  (CYD)
```

| Passo | Ligação |
|-------|---------|
| 1 | Carregador na GOOUUU (USB-C) |
| 2 | Jumper **5 V** GOOUUU → **P1 VIN** da CYD |
| 3 | Jumper **GND** GOOUUU → **GND** da CYD |
| 4 | UART (TX0/RX0 ↔ P1) |
| 5 | **CYD sem USB** durante operação normal |

> **Importante:** com a CYD alimentada pelo header, **não** plugue micro-USB nela ao mesmo tempo — evita back-feed entre duas fontes.

**Gravar firmware na CYD:** desligue o carregador da GOOUUU (ou desconecte o jumper 5 V), plugue **só** o micro-USB na CYD, flash, depois volte ao esquema acima.

**Monitor serial na CYD sem trocar cabos:** cabo **USB data-only** (sem pinos de alimentação) no micro-USB da CYD, com 5 V ainda vindo do jumper da GOOUUU. Se não tiver, desligue, flash/monitor, religue.

---

### Opção B — Uma fonte bench (recomendado montagem fixa)

Uma fonte **5 V / 2 A** com bornes ou barramento alimenta **as duas** pelos headers — **nenhum USB** na operação.

```
[Fonte 5V 2A]
   ├── 5 V → GOOUUU pin 5V
   ├── 5 V → CYD P1 VIN
   ├── GND → GOOUUU GND
   └── GND → CYD GND
```

| Passo | Ligação |
|-------|---------|
| 1 | Fonte off → ligar 5 V e GND nas duas placas |
| 2 | UART (TX0/RX0 ↔ P1) |
| 3 | Ligar a fonte |

**Gravar firmware:** desligue a fonte; conecte USB **em uma placa por vez** (a outra sem 5 V compartilhado ou desconectada do barramento).

---

### Opção C — Um USB com hub **com alimentação**

Um único cabo na tomada alimenta um **hub USB powered** (≥ 2 A no barrel do hub):

```
Tomada → [Hub powered 5V] ── USB-C → GOOUUU
                         └── micro-USB → CYD
              GND comum (jumpers) + UART
```

Ainda é **um ponto na tomada**; o hub distribui. Mantenha **GND** jumper entre as placas se o hub usar rails separados. UART igual às outras opções.

---

### Opção D — MB102 (USB Type-A → 5 V / 3,3 V) + breadboard

Módulo **MB102** — entrada **USB Type-A**, saídas **5 V** e **3,3 V** com jumpers. Ideal para prototipar o par num breadboard com **um cabo USB** na tomada.

```
Tomada → [Carregador USB 5V ≥2A] → USB-A → [ MB102 ]
                                                │
              jumper ESQ = 5V    jumper DIR = 5V │
                    │                         │
         trilho + ESQ (5V)      trilho + DIR (5V)   ← trilhos DIFERENTES
                    │                         │
              GOOUUU 5V              CYD P1 VIN
                    │                         │
         trilho − (GND comum) ────────────────┤
              GOOUUU GND              CYD P1 GND

         GOOUUU TX0/43 ──► P3 G35  |  RX0/44 ◄── P1 TX (G1)
```

#### Configuração dos jumpers MB102

| Jumper | Posição | Motivo |
|--------|---------|--------|
| Lado **esquerdo** (+/−) | **5 V** | GOOUUU precisa de **5 V** no VIN (LDO interno) |
| Lado **direito** (+/−) | **5 V** | CYD idem (P1 VIN / micro-USB = 5 V) |
| **3,3 V** | **Não use** para alimentar as placas | Só serve I²C/periféricos; ESP recebe 5 V |

> O MB102 **não** substitui o regulador das placas — ele só entrega 5 V como um carregador faria.

#### Ligação no breadboard

| De (MB102) | Para |
|------------|------|
| Trilho **+ esquerdo** (5 V) | GOOUUU pin **5V** |
| Trilho **+ direito** (5 V) | CYD **P1 VIN** |
| Trilho **− esquerdo** | GOOUUU **GND** |
| Trilho **− direito** | CYD **P1 GND** |
| **Jumper extra** | Trilho **− esq.** ↔ trilho **− dir.** *(obrigatório se GND separado)* |
| GOOUUU **TX0** / GPIO **43** | CYD **P3 GPIO 35** |
| GOOUUU **RX0** / GPIO **44** | CYD **P1 TX** (GPIO 1) |

> **5 V:** trilhos **+** diferentes (esq. / dir.) — ✅ correto.  
> **GND:** trilhos **−** parecem separados no breadboard, mas **têm que ser o mesmo net** para UART. O MB102 *deveria* unir os **−** por baixo; se na sua montagem cada placa foi só no **−** do seu lado, **ponte os dois trilhos − com um jumper**.

#### GND em trilhos diferentes — corrigir

```
        trilho + esq (5V) ── GOOUUU 5V
        trilho − esq (GND) ── GOOUUU GND ───┐
                                            ├── jumper GND (OBRIGATÓRIO)
        trilho − dir (GND) ── CYD P1 GND ───┘
        trilho + dir (5V) ── CYD P1 VIN
```

| Situação | UART |
|----------|------|
| GND só no **− esq.** (GOOUUU) e **− dir.** (CYD), **sem** ponte | ❌ Não funciona / lixo no serial |
| Jumper **− esq. ↔ − dir.** no breadboard | ✅ |
| Ou jumper direto **GOOUUU GND ↔ CYD P1 GND** | ✅ (dispensa ponte nos trilhos) |

**Teste rápido:** multímetro em continuidade entre GND da GOOUUU e GND da CYD — deve apitar. Se não apitar, falta o jumper.

> **Cada VIN em trilho 5 V diferente** — esquerdo = GOOUUU, direito = CYD. **GND sempre comum**, mesmo que em trilhos físicos distintos.

#### Corrente — atenção ao limite do MB102

Cada canal do MB102 usa regulador **AMS1117** (~**700 mA** máx. por trilho).

| Esquema | Viável? |
|---------|---------|
| **Duas placas no mesmo trilho 5 V** | ⚠️ Arriscado — pico do par ~1,2 A |
| **GOOUUU no trilho esq. · CYD no trilho dir.** | ✅ Recomendado — ~500 mA por canal |
| Carregador USB **≥ 2 A** na entrada MB102 | ✅ Obrigatório |

Se a CYD **reiniciar** ou o TFT **escurecer**, use carregador mais forte ou a [Opção A](#opção-a--um-usb-alimenta-as-duas-recomendado-em-dev) (derivar da GOOUUU).

#### BOM extra (Opção D)

| Peça | Qty |
|------|-----|
| MB102 power module (USB Type-A) | 1 |
| Breadboard 830 | 1 |
| Cabo USB-A (carregador → MB102) | 1 |
| Carregador **5 V / 2 A** | 1 |
| Jumpers M–M / M–F | vários |

**Operação:** MB102 ligado; **sem** USB nas placas ESP.  
**Flash:** desligue MB102; USB só na placa que está gravando (igual Opção B).

#### Uma placa por trilho — o que acontece?

```
        [ USB 5V ≥2A ]
              │
         ┌────┴────┐
         │  MB102  │
         └──┬───┬──┘
    AMS1117│   │AMS1117   ← dois reguladores separados
            │   │
    trilho +│   │+ trilho
    (5V esq)│   │(5V dir)
            │   │
         GOOUUU  CYD
            │   │
    trilho −┴───┴− trilho   ← GND **unido** no MB102
            (GND comum)
```

| Pergunta | Resposta |
|----------|----------|
| As placas ficam “isoladas”? | Só o **5 V** — cada uma tem seu regulador (~700 mA). |
| O **GND** é comum? | **Tem que ser.** MB102 une os **−** por baixo — se cada placa só no **−** do seu lado, **ponte − esq. ↔ − dir.** |
| Preciso jumper GND? | **Sim**, se GND está em trilhos **−** separados sem continuidade — 1 fio entre os trilhos **−** ou GOOUUU GND ↔ CYD GND |
| UART funciona? | **Só com GND comum** — TX/RX + ponte GND |
| Wi-Fi / câmera / TFT? | Funcionam normal; cada placa usa o **LDO dela** (5 V → 3,3 V **interno**). |

Ou seja: você divide **carga de 5 V**; **não** divide referência de terra.

#### E o trilho 3,3 V do MB102?

**Para o edge pair: não liga em lugar nenhum nas duas placas.**

| Entrada de energia | O que a placa faz |
|--------------------|-------------------|
| GOOUUU pin **5V** | LDO onboard → 3,3 V para ESP-S3, câmera, SD |
| CYD **P1 VIN** (5 V) | Dois LDO onboard → 3,3 V para ESP32 e TFT |

O trilho **3,3 V** do MB102 só entra se você adicionar **periférico externo** 3,3 V (ex.: sensor I²C, OLED avulso):

```
MB102 trilho 3,3 V  →  VDD do sensor / OLED
MB102 trilho GND    →  GND do periférico
                      (não substitui o 5 V da placa ESP)
```

| Jumper MB102 | Edge pair (S3 + CYD) |
|--------------|----------------------|
| **5 V** (esq. + dir.) | ✅ Alimenta as placas |
| **3,3 V** | ⬜ Deixe sem uso — ou só periférico extra |

**Nunca** alimente GOOUUU ou CYD pelo trilho **3,3 V** do MB102 — corrente insuficiente e pinos errados (elas esperam **5 V** no VIN).

---

### Regras de ouro

| ✅ Faça | ❌ Não faça |
|---------|-------------|
| Um único 5 V compartilhado | Duas fontes independentes sem GND comum |
| GND comum (energia + UART) | USB na CYD **e** jumper 5 V ao mesmo tempo |
| Fonte ≥ **2 A** | Carregador 500 mA de celular antigo |
| Desligar barra antes de flash | USB GOOUUU + USB CYD + jumpers 5 V juntos |

---

### Programação vs operação

| Modo | GOOUUU | CYD |
|------|--------|-----|
| **Operação** (Opção A) | USB-C carregador | Só jumper 5 V/GND |
| **Operação** (Opção B) | Header 5 V | P1 VIN |
| **Flash GOOUUU** | USB-C data | Desligada ou sem 5 V |
| **Flash CYD** | Pode desligada | micro-USB data |

Referência GOOUUU (bench vs USB): [mini-games/HARDWARE.md](../goouuu-esp32-s3-cam/mini-games/HARDWARE.md).

---

## Parâmetros UART

| Parâmetro | Valor |
|-----------|-------|
| Baud rate | **460800** (`PEER_UART_BAUD`) |
| Formato | 8N1 (8 bits, sem paridade, 1 stop) |
| Fluxo | Sem RTS/CTS (3 fios: TX, RX, GND) |

Monitor USB de debug: **115200** (UART CH340, independente do link peer).

Se houver corrupção no link peer, teste **115200** ou cabo mais curto (&lt; 20 cm).

---

## Firmware (referência)

### S3-CAM — `HardwareSerial`

```cpp
// edge-s3 — pins.h
#define PIN_PEER_TX  43  // silk TX0
#define PIN_PEER_RX  44  // silk RX0

HardwareSerial PeerSerial(1);  // UART1

void peerUartBegin() {
  PeerSerial.begin(PEER_UART_BAUD, SERIAL_8N1, PIN_PEER_RX, PIN_PEER_TX);
}
```

### CYD — `Serial2` remapeado

```cpp
// edge-cyd — pins.h
#define PIN_PEER_RX  35   // input only
#define PIN_PEER_TX  22

void peerUartBegin() {
  Serial2.begin(PEER_UART_BAUD, SERIAL_8N1, PIN_PEER_RX, PIN_PEER_TX);
}
```

### Teste de loopback lógico

**S3** envia:

```cpp
PeerSerial.println("PAIR:HELLO from S3");
```

**CYD** responde:

```cpp
if (Serial2.available()) {
  String line = Serial2.readStringUntil('\n');
  Serial2.println("ACK:" + line);
}
```

Monitore pela USB **só durante desenvolvimento** — ver [Programação vs operação](#programação-vs-operação). Na operação com alimentação única, a CYD em geral **não** tem USB plugado.

---

## Checklist de montagem

- [ ] **Um** ponto de energia (USB ≥ 2 A ou fonte 5 V ≥ 2 A)
- [ ] **5 V** compartilhado: GOOUUU header → CYD P1 VIN
- [ ] **GND** comum (energia + UART)
- [ ] GOOUUU **TX0** (GPIO **43**) → CYD **P3 GPIO 35**
- [ ] GOOUUU **RX0** (GPIO **44**) ← CYD **P1 TX** (GPIO 1)
- [ ] CYD **sem** micro-USB na operação (Opção A/B)
- [ ] CYD **P1 RX (GPIO 3) sem fio** — alimentação + TX no P1; RX no P3
- [ ] Baud **460800** nos dois firmwares (`PEER_UART_BAUD`)
- [ ] Teste `PAIR:HELLO` / `PAIR:OK`

---

## Diagrama completo (par + PC)

```
              [ UM PONTO: USB 5V 2A ou fonte bench ]
                              │
              ┌───────────────┴───────────────┐
              │ 5V + GND (barra comum)       │
              ▼                               ▼
   ┌──────────────────────┐       ┌──────────────────────┐
   │ GOOUUU S3-CAM        │ UART  │ CYD 2432S028         │
   │ (entrada principal)  │◄────►│ (5V derivado / VIN)  │
   │ Wi-Fi ───────────────┼───┐   │ Wi-Fi ───────────┐   │
   └──────────────────────┘   │   └──────────────────┘   │
                              │                          │
                              └──────────┬───────────────┘
                                         ▼
                                    [ PC / LAN ]
```

---

## Problemas comuns

| Sintoma | Causa provável | Correção |
|---------|----------------|----------|
| Lixo no serial | TX↔TX ou RX↔RX | Cruzar TX→RX |
| Nada chega | Sem GND comum | Ligar GND das duas |
| CYD não responde | P1 RX (GPIO 3) usado | S3 TX → **P3 GPIO 35**; P1 RX sem fio |
| S3 trava ao boot | GPIO strapping | Não usar 0, 3, 46 |
| Dados corrompidos | Cabo longo / baud alto | Cabo &lt; 20 cm; reduzir para 115200 se necessário |
| LED CYD piscando weird | TX na GPIO 16/17 | Usar 22/35 |
| CYD reinicia ao ligar TFT | Fonte fraca (&lt; 1 A) | Usar **5 V ≥ 2 A**; no MB102, **uma placa por trilho** |
| MB102 esquenta / cai tensão | Par no mesmo trilho 5 V | GOOUUU trilho esq. · CYD trilho dir. |
| Cheiro de USB / quente | Duas fontes na CYD | Só jumper **ou** só USB, nunca os dois |

---

## Próximo passo

Formato das mensagens (texto only, sem binário): [PROTOCOL.md](PROTOCOL.md).

Visão geral da arquitetura: [README.md](README.md).
