# PCB Analysis YAML — AI Assistant Guide

Upload `pcb_analysis.yaml` to an AI chatbot and have a technical conversation about your board — design review, signal integrity, power, EMC, BOM, firmware bring-up. The AI gets full context in one file.

Component datasheets are in `6_DOC/datasheets/`.

---

## Quick Start

1. Open [claude.ai](https://claude.ai) (or any capable AI chatbot)
2. Upload `pcb_analysis.yaml` as an attachment
3. Start broad: *"Assess this PCB. Project description is at https://github.com/koendv/arm_can_tool"*

The AI now has full context and you can drill down from there.

---

## Example Prompts

> *"What items in the design checklist still require action before I send to fab?"*

> *"What are the main functional blocks of this board and how are they connected?"*

> *"Which nets need controlled impedance routing, and what trace width should I use?"*

> *"Are there any assembly notes I need to communicate to the fab?"*

> *"I've attached the datasheets for U5 (SD8931) and its alternate (DS3231MZ). Compare and assess what would need to change to swap them."* (Note: the DS3231MZ datasheet is in English, the SD8931 in Chinese — the AI handles both.)

> *"I've attached the CA-IS2062VW datasheet. Check the schematic for U3 against the reference application circuit."*

> *"Assume CAN bus transceiver creepage is 9mm, clearance is 7mm, and PCB has CTI≥175V. Assess according to IEC 60664-1."*

---

## Tips for Better Conversations

**Be specific about the subsystem.** The AI can focus better when you name a block:
> *"Focus on the I2C bus. Which devices share I2C1, and are the pull-up resistors sized correctly?"*

**Ask for a checklist.**
> *"Create a pre-fab checklist based on the open items in this design."*

**Use the embedded datasheets.** Every component in the YAML includes a datasheet URL. Ask the AI to fetch and cross-reference them:
> *"Check the datasheet for U3 and verify the decoupling capacitors match the recommended application circuit."*

**Attach additional documents sparingly.** You can upload datasheets or reference manual sections alongside the YAML. Avoid uploading an entire MCU reference manual — extract and upload only the relevant section, for example the QSPI/XIP chapter when working on flash memory bring-up.

**Use alternate parts for cost reduction.** The YAML includes schematic-documented alternates with LCSC part numbers. Two levels of search:

Focused — uses only designer-validated alternates already in the schematic:
> *"Using the alternate parts listed in the schematic, consider replacing parts with Chinese domestic alternatives. Give BOM savings."*

Open — broader search for any cheaper compatible part on LCSC:
> *"Search LCSC for cheaper Chinese domestic alternatives to any part in the BOM. Include the schematic's documented alternates and any other compatible parts you find. Give BOM savings."*

The focused query is safer and faster. The open query may find additional savings but results need engineering review before production.

**Iterate.** The AI retains the YAML context for the whole conversation. You can ask follow-up questions without re-uploading.

---

## Generating pcb_analysis.yaml

To regenerate this file for this board, or create one for a different EasyEDA Pro project:

1. Export BOM, netlist, and pick & place files from EasyEDA Pro
2. Run [easyeda_parser](https://github.com/koendv/easyeda_parser)
3. Upload the schematic PDF and the parser output
4. Merge into `pcb_analysis.yaml`

---

*Project: `arm_can_tool` · Author: koendv · License: [CC0 Public Domain](https://creativecommons.org/publicdomain/zero/1.0/)*
