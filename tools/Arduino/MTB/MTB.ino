/*
 * Micro Trace Buffer (MTB) test program for SAMD21/SAMC21 (Cortex-M0+).
 *
 * When program execution is not sequential, MTB logs two addresses to a
 * circular buffer in ram: the address the program jumped from and the
 * address the program jumped to. This program reserves a 1 KB buffer,
 * switches MTB on, and then solves Towers of Hanoi so there is a recursion
 * to see in the trace. Moves are printed on Serial1.
 *
 * Read the trace with arm-none-eabi-gdb-py3:
 *
 *   (gdb) break hanoi if n == 0
 *   (gdb) run
 *   (gdb) mon mtb status
 *   (gdb) source tools/mtb/mtb.py
 *   (gdb) mtb
 *
 * Build:
 * SAMD21:
 *   arduino-cli compile --fqbn arduino:samd:arduino_zero_native \
 *     --build-property "build.ldscript=linker_scripts/gcc/flash_without_bootloader.ld" \
 *     --export-binaries tools/Arduino/MTB
 *
 * SAMC21:
 *   arduino-cli compile --fqbn xplainedniladri:samc:samc21_xplained_Niladri \
 *     --export-binaries tools/Arduino/MTB
 *
 * The SAMD21 build overrides the linker script because the firmware is loaded
 * over SWD, not through the SAM-BA bootloader.
 *
 * See doc/DEBUG.md, section MTB.
 */

/*
 * MTB_SFR_BASE is the address of the MTB registers. It differs between chips.
 * To find MTB_SFR_BASE for a new chip, attach with gdb and run `mon mtb status`.
 * The value printed after "mtb base:" is MTB_SFR_BASE.
 */
#if defined(__SAMD21G18A__)
#define MTB_SFR_BASE 0x41006000UL
#elif defined(__SAMC21J18A__) || defined(__SAMC21N18A__)
#define MTB_SFR_BASE 0x41008000UL
#else
#error "MTB_SFR_BASE unknown for this chip. Run `mon mtb status` and add a case here."
#endif

/* MTB registers, ARM DDI 0486B table 3-1 */
#define REG_MTB_POSITION (*(volatile uint32_t *)(MTB_SFR_BASE + 0x0)) /* write pointer into buffer */
#define REG_MTB_MASTER   (*(volatile uint32_t *)(MTB_SFR_BASE + 0x4)) /* enable, buffer size */
#define REG_MTB_FLOW     (*(volatile uint32_t *)(MTB_SFR_BASE + 0x8)) /* watermark, auto stop */
#define REG_MTB_BASE     (*(volatile uint32_t *)(MTB_SFR_BASE + 0xC)) /* SRAM base address, read-only */

#define MTB_MASTER_EN   0x80000000UL
#define MTB_MASTER_MASK 6UL /* buffer size = 2^(MASK+4) = 1024 bytes */

/*
 * The trace buffer. The size must match MTB_MASTER_MASK, and the buffer must
 * be aligned to its own size. Reserving the buffer in the program, rather
 * than from the debugger, guarantees it does not overlap other variables.
 */
#define MTB_BUFFER_WORDS 256 /* 256 * 4 = 1024 bytes */

__attribute__((aligned(MTB_BUFFER_WORDS * sizeof(uint32_t))))
static uint32_t mtb_buffer[MTB_BUFFER_WORDS];

/*
 * Switch MTB on. POSITION is the buffer offset from the start of SRAM.
 * The start of SRAM is read from MTB_BASE rather than hardcoded.
 */
static void mtb_enable() {
  const uint32_t sram_base = REG_MTB_BASE;
  const uint32_t buffer_addr = (uint32_t)(uintptr_t)mtb_buffer;
  REG_MTB_MASTER = MTB_MASTER_MASK; /* EN = 0 while changing POSITION */
  REG_MTB_POSITION = (buffer_addr - sram_base) & 0xFFFFFFF8UL;
  REG_MTB_FLOW = 0; /* no watermark: buffer wraps around, oldest jumps are overwritten */
  REG_MTB_MASTER = MTB_MASTER_EN | MTB_MASTER_MASK;
}

/*
 * On hard fault, switch MTB off. The buffer then holds the jumps that led
 * to the fault. Attach with gdb and run `mtb` to see them.
 */
extern "C" void HardFault_Handler(void) {
  REG_MTB_MASTER = MTB_MASTER_MASK; /* EN = 0 */
  while (true) {}
}

/*
 * Towers of Hanoi. Recursion gives calls and returns at varying depth.
 * Break at the deepest level with: break hanoi if n == 0
 */
void hanoi(int n, int from, int to, int via) {
  if (n == 0) return;
  hanoi(n - 1, from, via, to);
  Serial1.print("move disk ");
  Serial1.print(n);
  Serial1.print(" from ");
  Serial1.print(from);
  Serial1.print(" to ");
  Serial1.println(to);
  hanoi(n - 1, via, to, from);
}

void setup() {
  Serial1.begin(115200);
  mtb_enable();
}

void loop() {
  hanoi(4, 1, 3, 2);
  Serial1.println();
  delay(1000);
}

// not truncated
