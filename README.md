
TODOs:
* fully isolate ESP32 from relay/mains side (see [Learnings](#learnings))
* compile and dependecies
* implement schedule
* read temp sensor
* read humidity sensor
* housing for esp32s and sensors?
  * electrical box?
  * 3D print???
  * lots of electrical tape????
* BLE or wifi?
* sensor per plant / sensor?
* configuration file
  * schedule
  * temperatures

* motor / water pump
* think about water management / flooding


## Learnings
* GPIO silkscreen labels on this board match the actual GPIO numbers 1:1 (e.g. the pin printed "D25" is GPIO25) — confirmed by watching a multimeter toggle in sync with the serial log while the firmware ran. An earlier comment assuming an off-by-one ("GPIO is n-1") was wrong.
* The relay module needs 5V on VCC to power the coil driver. Pulling the VCC/JD-VCC jumper to give the coil its own dedicated supply works, but the module only has one shared GND pin (no separate GND for JD-VCC) — that dedicated supply's ground still has to tie into the same ground as the ESP32/Pi/module, or the coil has no return path. Misleading failure mode: the opto-isolator LED still lights even when the coil circuit is broken this way. Keeping the jumper in place is the simplest correct wiring unless true isolation gets implemented (see TODO above).
* The relay is active-LOW: driving IN LOW energizes it (COM switches NC → NO); HIGH or floating at boot leaves it de-energized. The firmware's "ON"/"OFF" log labels are currently inverted relative to this.

## Hardware
* [esp32 dev kit](https://www.amazon.com/dp/B0BVZBTP8V?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_3)
* [breadboard power supply](https://www.amazon.com/dp/B0BP9V6WXX?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_4&th=1)

