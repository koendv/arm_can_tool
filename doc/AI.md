# ARM CAN TOOL — AI-Assisted Design Review

> AI is a slippery tool. It has to be grasped firmly.

Upload `Hardware/V1.0/6_DOC/pcb_analysis.yaml` to an AI assistant and conduct a technical design review — signal integrity, power, EMC, BOM, firmware bring-up. The AI receives full hardware context in one file.

Component datasheets are in `Hardware/V1.0/6_DOC/datasheets/`.

---

## pcb\_analysis.yaml Contents

BOM, pick-and-place, and netlist.

---

## Quick Start

1. Open AI assistant.
2. Upload `pcb_analysis.yaml` as an attachment.
3. Paste the opening prompt below.
4. Begin asking technical questions.

**Opening prompt — paste verbatim at the start of every session:**

```
pcb_analysis.yaml has been uploaded.

1. Confirm the file is not truncated. What is the last section you can see?

2. Datasheets are authoritative only in what they explicitly state. Do not infer anything not documented. If a value or behaviour is unspecified, say so.

3. If you are uncertain about any answer, say so explicitly.
```

---

## File Verification

Each conversation with an AI is a stream of tokens. As the conversation progresses, the token window slides forward and earlier context is forgotten. Window size varies by AI assistant and by subscription level within the same assistant.

Before starting a session, query the AI for its current context window size:

> `What is your current context window size?`

The uploaded file occupies the start of the context window. Each exchange adds tokens. As the conversation grows, the window fills and early context — including parts of the uploaded file — may be silently dropped. Use the session YAML pattern to save state and restart before this occurs.

Context window size is not the only factor. Technical reasoning quality, file handling reliability, and the ability to fetch and cross-reference URLs all affect suitability for hardware design review.

Verify `pcb_analysis.yaml` was received complete before asking any technical questions. A silently truncated file produces incorrect answers that are difficult to detect.

The truncation check in the opening prompt is mandatory. Do not skip it.

If the AI reports truncation, regenerate with a smaller token limit:

```
easyeda_parser --token-limit <smaller_value> ...
```

See [github.com/koendv/easyeda_parser](https://github.com/koendv/easyeda_parser) for options.

---

## Session Pattern

Without constraints, AI assistants generate readily and confidently, but output is unreliable. The following pattern prevents this.

For any query:

```
Do not generate yet. First discuss.
<state query>
Is my intent clear?
Do any questions remain?
Do you need additional data?
```

> Additional data may include datasheets, firmware source files, or HAL libraries.

When satisfied:

```
Proceed.
```

Apply this pattern consistently. It is the primary discipline for getting reliable output from an AI assistant.

---

## Session Backups

After completing a subsystem review, or before starting a new topic — make a backup:

```
Give me the session YAML so I can resume this conversation in another session.
```

Download the YAML to your PC. To return to the discussion at the point where the session YAML was generated, open a new session and upload the session YAML.

---

## Session Limit

As the conversation grows, the context window fills and early context may be silently dropped. Query the AI periodically to check status:

> `How are we for context window?`

When the window is near capacity, save a session backup and start a new session.

---

## Example Prompts

**Design overview:**

> `Assess this PCB. Project description is at https://github.com/koendv/arm_can_tool`

**Component verification:**

> `I have attached the datasheets for U5 (SD8931) and its alternate (DS3231MZ). Compare and assess what would need to change to swap them.`

(Note: the DS3231MZ datasheet is in English, the SD8931 datasheet in Chinese.)

**Fault diagnosis:**

> `No communication with target, but software reports target VIO correctly. How do I repair?`

**Standards assessment:**

> `Assume CAN bus transceiver creepage is 9 mm, clearance is 7 mm, and PCB has CTI ≥ 175 V. Assess according to IEC 60664-1.`

Tip: upload the relevant section of the standard PDF alongside `pcb_analysis.yaml`. The AI cross-references the design against the standard directly.

**BOM cost reduction:**

1. Go to the JLCPCB parts list and search for the component to replace.
2. Click on its category — this filters to all components in that category.
3. Sort by stock (most in stock first).
4. Copy the list.
5. Paste into the AI session with the following query:

    > `Here is the JLCPCB parts list for [component category], sorted by stock. I am considering replacing [current part]. Identify compatible drop-in or pin-compatible alternatives. Give BOM savings.`
    
6. Engineering review.

Sorting by stock filters out low-volume and hard-to-source parts before the AI sees the list. 

---

## Tips

**Reinforce the datasheet constraint when asking about specific components.**
The opening prompt sets the constraint for the session. Reinforce it when needed:

> `The datasheet is authoritative in what it states. Do not infer anything it does not explicitly state. If a value or behaviour is not documented, say so.`

**Ask for uncertainty to be stated explicitly.**

A confident wrong answer is more dangerous than a hedged correct one. If the AI gives a confident answer that seems uncertain, ask directly:

> `How confident are you in that answer? What is it based on?`

**Name the subsystem.**

> `Focus on the I2C bus. Which devices share I2C1. Are the pull-up resistors sized correctly?`

**Use the embedded datasheets.**

Every component in `pcb_analysis.yaml` includes a datasheet URL. Ask the AI to fetch and cross-reference:

> `Check the datasheets of all components connected to the 5V rail. What is the maximum voltage?`

**Attach additional documents sparingly.**

Upload only the relevant section of a reference manual, not the entire document. Example: when working on flash memory bring-up, upload only the QSPI/XIP chapter of the MCU reference manual.

**The AI retains context within the session window.**

Ask follow-up questions without re-uploading `pcb_analysis.yaml`. For long sessions, use session backups before the window fills.

---

## pcb_analysis.yaml Limitations

Schematic text and component annotations are not yet extracted. Trace width and length data from the Gerbers are not yet included.

Where trace geometry is required, supply the data manually in the query.

---

## Generating pcb_analysis.yaml

1. Export BOM, netlist, and pick-and-place files from EasyEDA Pro.
2. Run [easyeda_parser](https://github.com/koendv/easyeda_parser).
3. Upload the schematic PDF and the parser output.
4. Begin session.

If the resulting file exceeds the AI context window, regenerate with a smaller `--token-limit`.
