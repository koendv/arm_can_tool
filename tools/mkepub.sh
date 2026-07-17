#!/bin/bash

#
# convert README.md from markdown to epub
# run from repository root:
# ./tools/mkepub.sh
# creates doc/arm_can_tool.epub
#

#
# convert schematic pdf to png
#
# pdftoppm -r 300 -mono -png  Hardware/V1.0/1_SCH/SCH_Schematic1_2026-04-18.pdf doc/pictures/schematic
# optipng -o3 doc/pictures/schematic-*.png

SOURCES=(
  README.md
  doc/INSTALL.md
  doc/TUTORIAL.md
  doc/OPERATION.md
  doc/DEBUG.md
  doc/TARGETS.md
  doc/CANBUS.md
  doc/SCRIPT.md
  doc/LOGGING.md
  doc/HARDWARE.md
  doc/SCHEMATIC.md
  doc/AI.md
  doc/REMOTE.md
  doc/DEVELOPER.md
  doc/MANUFACTURING.md
  LICENSE.md
  doc/COMMERCIAL.md
  doc/REFERENCE.md
  doc/LUA_REF.md
)

cat "${SOURCES[@]}" > doc/arm_can_tool.md

pandoc \
  "${SOURCES[@]}" \
  -o doc/arm_can_tool.epub \
  --lua-filter=tools/fix-links.lua \
  -M base_url="https://github.com/koendv/arm_can_tool/blob/main/" \
  --metadata title="ARM CAN TOOL" \
  --metadata author="koendv" \
  --metadata identifier="urn:uuid:$(uuidgen)" \
  --toc \
  --toc-depth=2 \
  --resource-path=.:doc:doc/pictures \
  --standalone \
  --epub-cover-image=doc/pictures/handbook.png

