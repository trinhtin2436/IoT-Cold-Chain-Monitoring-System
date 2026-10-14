<div align="center">

## Project 07: Cold-Chain Monitoring and Intelligent Storage Management
**Difficulty 5–6/10 | Team 4–5 students | End-to-End IoT System**

</div>

---

**Project Objective**  
Build a cold-chain prototype that monitors environmental compliance over time, detects excursions, preserves traceable history, and distinguishes short disturbances from sustained violations. **Mandatory Hardware and Physical-System Requirements**

* ESP32-class node; temperature and humidity sensing; door/open-close sensing is mandatory; optional second temperature/light sensor is encouraged.
* Justify sensor placement and compare/calibrate against a reference thermometer.
* Optional fan/cooling simulator may be controlled using safe low-voltage hardware.

**Mandatory IoT, Backend, and Application Requirements**

* Publish timestamped telemetry/door events through MQTT with availability; persist compliance-relevant history.
* Implement accumulated degree-time, excursion duration, or another justified duration-aware quality metric; instantaneous threshold alone is insufficient.
* Dashboard: current conditions, history, door events, exposure status, data freshness, alarms, and connectivity.
* REST API must support historical/compliance queries; distinguish warning from sustained/critical excursion.
* Use local buffering or local exposure calculation so network outage does not erase compliance information.

**Required Advanced Component**  
Implement multi-level excursion severity jointly based on temperature deviation and duration, or compare two sensor locations for thermal non-uniformity. **Experimental Evaluation — Mandatory**

* Perform normal-storage, door-open, warming/cooling-loss, and network-outage experiments.
* Report sensor error, alarm delay, exposure calculation, data-loss/recovery performance, and false alarms.
* Experimentally demonstrate the added value of duration-aware exposure over an instantaneous threshold.

**Live Demonstration Requirement**  
Create a controlled temperature excursion; show classification, alarm/history, and preservation/recovery of compliance state after network interruption. **Common Mandatory Deliverables**

* **Working integrated prototype and source code:** embedded firmware, backend/data services, and dashboard/application code must be organized and reproducible. Sensor-to-dashboard only is incomplete.
* **Report:** Introduction/Problem; Requirements and Theory; Architecture; Hardware; Protocol/Networking; Backend/Data; Application/UI; Control/Intelligence; Security/Reliability; Experimental Method; Results/Discussion; Limitations; Conclusion; References; and member contributions.
* **60-minute presentation including Q&A:** recommended 35–40 min technical presentation, 10–15 min live demo, remaining time for questions. Every member must present and answer technical questions.
* **Technical evidence:** architecture and wiring diagrams, MQTT/API specification, database schema, source repository, screenshots, experiment tables/plots, and backup demo video.

**Minimum Quality Bar**  
Assessment emphasizes end-to-end correctness, embedded/hardware quality, communication/backend/data design, application quality, control/intelligence, reliability/security, experimental rigor, report/presentation quality, and individual technical understanding. Features that are not demonstrated or evaluated do not receive full technical credit.
