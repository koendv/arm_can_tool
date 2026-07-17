# Manufacturing Instructions

## Quick Order Checklist

1. PCB: `Hardware/V1.0/4_GERBER/Gerber_PCB1_2026-04-13.zip`
2. BOM: `Hardware/V1.0/3_BOM/BOM_Board1_PCB1_2026-04-13.xlsx`
3. Pick & Place: `Hardware/V1.0/3_BOM/PickAndPlace_PCB1_2026-04-13.xlsx`
4. Enclosure STL: `Hardware/V1.0/5_3D/enclosure/`
5. Firmware: `Hardware/V1.0/7_FW/

## Assembly Notes

| component | note                                                   |
+-----------+--------------------------------------------------------|
| BT1       | No solder paste on middle pad of battery connector BT1 |
| OLED1     | DNS.                                                   |

## User responsibility

The PCB is shipped without pre-programmed firmware. The user is responsible for:

1. attaching OLED display (double-sided adhesive tape, FPC connector)
2. installing UF2 bootloader (one time, via DFU)
3. installing application firmware (routine, via USB copy)
4. 3D-printing the enclosure (SLA)
