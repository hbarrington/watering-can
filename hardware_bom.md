# Hardware BOM

Kept up to date as parts get picked, ordered, or replaced. Costs are approximate street prices at time of writing (Sep 2026) — verify before ordering. "Status" tracks where each line actually is: **owned**, **needed** (decided, not yet bought), or **pilot** (buy 1 to validate before committing to the full production quantity).

## Development / Prototype BOM
What's needed to get phases 1–4 (see README Roadmap) working on a single tree before scaling to 4.

| Item | Qty | Status | Notes | Link |
|---|---|---|---|---|
| ESP32 dev board (LAFVIN basic starter kit) | 1+ | owned | Kit also includes the dual relay module, breadboard, jumper wires, and misc basic components used in `samples/relay-demo`. | [Amazon](https://www.amazon.com/LAFVIN-Starter-Development-Tutorial-Compatible/dp/B0BVZBTP8V) |
| Breadboard power supply module | 1 | owned | Used as the dedicated 5V source for the relay's VCC/JD-VCC when isolating relay power from logic power (see main README Learnings). | [Amazon](https://www.amazon.com/dp/B0BP9V6WXX) |
| DHT11 temp/humidity sensor | 1 | owned | Ambient air only — see main README Learnings for why this doesn't cover root-zone temp or soil moisture. | — |
| Raspberry Pi ("watering-can" on the network) | 1 | owned | USB-attached to the ESP32; used to flash and monitor, not part of the running system. | — |
| M5Stack Unit PoE-P4 (pilot pot node) | 1 (of 3 owned; 1 more to buy for the 4th tree) | owned | The sensors below wire into this, not the main ESP32 — it's the actual pot-module hardware. See the Production BOM row for full specs. | [M5Stack](https://shop.m5stack.com/products/unit-poe-with-esp32-p4) |
| DS18B20 waterproof temperature probe | 1 | needed | Root-zone/soil temperature, one per tree in production. Digital (OneWire), no calibration needed. ~$3–11 depending on source/pack size. | [SparkFun](https://www.sparkfun.com/temperature-sensor-waterproof-ds18b20.html) ($10.95 single) |
| Capacitive soil moisture sensor | 1 | needed | Analog, 3.3–5.5V. Get the **capacitive** kind, not the cheap two-prong resistive probes — resistive probes corrode within months of continuous soil contact. ~$2–8 each, usually cheaper in multi-packs. | [DIYables](https://diyables.io/products/capacitive-soil-moisture-sensor-module) |
| Soil pH sensor — DFRobot Gravity Analog Spear Tip pH Kit (SEN0249) | 1 | pilot | See "Sensor recommendation" below. $99 — pilot on one tree before buying 3 more. | [DFRobot](https://www.dfrobot.com/product-1668.html) |
| Small water pump (5V/12V submersible) | 1 | needed | For the automatic-watering phase. Needs a driver (relay or MOSFET) sized for its voltage/current, not the 120V relay module. | — |
| 120V warming blanket / seedling heat mat | 1 | needed | For the root-warming phase. Same relay-switching pattern already proven for grow lights. | — |

## Production BOM (4 trees + 1 light fixture)
Quantities assume the full build-out described in the System Diagram: 1 main module in the ceiling fixture, 4 independent pot modules.

| Item | Qty | Notes |
|---|---|---|
| Main ESP32 module (ceiling fixture) | 1 | Runs the grow-light schedule; can reuse a dev-kit board. Stays on WiFi (`src/`) — it's not in the wired-Ethernet run. |
| Relay bank for grow lights | 1 (2–4 ch) | Matches `CONFIG_WC_NUM_LIGHT_RELAYS` in `src/`. |
| Pot controller module — M5Stack Unit PoE-P4 | 4 (3 owned, 1 to buy) | ESP32-P4 (RISC-V, 360MHz dual-core, 16MB flash, 32MB PSRAM), wired 10/100 Ethernet via onboard IP101GRI PHY, powered over the same cable via 802.3at PoE (6W budget — plenty for the sensors + relay-coil logic here, mains power for the pump/heat mat itself comes from their own 120V circuits, not PoE). No WiFi/BT on this chip. $21.50 each. One HY2.0-4P Grove port plus a 16-pin "Hat2-Bus" GPIO header; exact pin mapping for ADC/I2C/OneWire still needs to be looked up against the datasheet before wiring sensors. Different chip family from the main module's ESP32, so it needs its own `idf.py set-target esp32p4` firmware project — plan to share components rather than duplicate. |
| DS18B20 waterproof probe | 4 | One per pot, root-zone temperature. |
| Capacitive soil moisture sensor | 4 | One per pot. |
| Soil pH sensor (DFRobot SEN0249 or equivalent) | 4 | Only buy the remaining 3 once the pilot unit proves out — see Sensor recommendation. |
| Water pump + driver | 4 | One per pot, unless a shared-pump/per-pot-valve design is chosen instead (open question — plumbing not yet designed). |
| 120V relay + warming blanket | 4 | One per pot. |
| Waterproof enclosure | 4 | For the pot modules — they're living next to soil and a water source. |
| Enclosure | 1 | For the main module, sized to fit in/near the ceiling light fixture. |

## Sensor recommendation: soil acidity (pH)

Recommending the **DFRobot Gravity Analog Spear Tip pH Sensor/Meter Kit (SEN0249)**:
- Purpose-built for direct insertion into soil (stainless spear tip), unlike the more common glass-bulb pH probes meant for liquid/hydroponic reservoirs.
- 0–10 pH range, ±0.1 pH (probe) / <±0.2 pH (board) accuracy, analog output.
- **$99** — the most expensive sensor in the system by a wide margin, and probably more precision than a potted calamansi tree strictly needs.
- **Wiring note:** it outputs 0–4.0V, but the ESP32's ADC tops out at 3.3V — needs a resistor voltage divider (or the board's onboard attenuation, if it has one) before wiring to a GPIO, or you'll clip/damage the input at the high end of its range.
- Calamansi (citrus) generally wants slightly acidic soil, roughly pH 6.0–6.5, for reference once readings start coming in.

Given the cost, pilot with **one** unit before buying three more. Soil pH also drifts far more slowly than moisture, so unlike watering, it may not need continuous automated response at all — a $10–15 handheld soil pH meter used for occasional manual spot-checks on the other three trees is a reasonable (much cheaper) fallback if the automated sensor doesn't earn its keep.

Sources:
- [M5Stack Unit PoE-P4](https://shop.m5stack.com/products/unit-poe-with-esp32-p4) / [docs](https://docs.m5stack.com/en/unit/Unit_PoE-P4)
- [DFRobot Gravity Analog Spear Tip pH Sensor/Meter Kit (SEN0249)](https://www.dfrobot.com/product-1668.html)
- [SparkFun Waterproof DS18B20 Temperature Sensor](https://www.sparkfun.com/temperature-sensor-waterproof-ds18b20.html)
- [DIYables Capacitive Soil Moisture Sensor Module](https://diyables.io/products/capacitive-soil-moisture-sensor-module)
- [LAFVIN Basic Starter Kit for ESP32 (Amazon)](https://www.amazon.com/LAFVIN-Starter-Development-Tutorial-Compatible/dp/B0BVZBTP8V)
