/**
 * V. Hunter Adams (vha3@cornell.edu) | SD FatFS Implementation - Julien Ferand - Dokitek (julien@dokitek.com)
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "hardware/sync.h"
#include "sd.h"
#include "ff.h"

// Comment ouf if you want to disable the 5 second delay
#define DEBUG_STDIO_DELAY

// On Pico W, change to another value and wire a LED to that pin
// We won't include the CYW43 library here just to toggle a LED, it's way too flash expensive
#define LED_PIN 25
#define BOOTLOAD_PIN 14

// Sector and page sizes for flash memory
#define SECTOR_SIZE 4096
#define PAGE_SIZE 256

// Application program offset in flash (128k)
// This should agree with the linker script for the application program.
#define PROGRAM_OFFSET 131072

// Filename of the firmware binary on the SD card
// We won't use .hex files as the original parser won't accept too large files, and SD communication is reliable enough
// TODO - Implement a checksum verification for the firmware file, tricky as we're using a binary file with no line by line checksums
#define SD_FIRMWARE_FILENAME "firmware.bin"

// For keeping track of where we are in programming/erasing flash
uint32_t programming_address;
uint32_t erasure_address;

// Address of binary information header
uint8_t *flash_target_contents = (uint8_t *) (XIP_BASE + PROGRAM_OFFSET + 0xD4);

// We'll claim a DMA channel
int dma_chan_1; // Clears flashbuffer
int dma_chan_2; // Copies flashbuffer to first_page_buffer

// For counting characters received
int char_count;

// For zeroing-out receive buffer
char zeroval = 0x00;

// flash page buffer
uint8_t flashbuffer[256]{0};

// We're going to program the first page LAST, so we don't
// accidentally vector into a partially-programmed application
unsigned char first_page{0};
unsigned char first_page_buffer[PAGE_SIZE]{0};

bool fileStartAlreadyHandled{false};

void deinitCommonResources() {
    printf("[deinitCommonResources] - Cleaning up common resources...\n");
    gpio_deinit(LED_PIN);
    gpio_deinit(BOOTLOAD_PIN);
    printf("[deinitCommonResources] - Releasing STDIO, see you on the other side!\n");
    stdio_deinit_all();
}

// Initializes erasure address and programming address.
// Clears the flashbuffer
// Erases the first sector of flash, and increments erasure
// address by the sector size (4096 bytes).
static inline void handleFileStart() {
    // Reset the erasure pointer
    erasure_address = PROGRAM_OFFSET;

    // Reset the programming pointer
    programming_address = PROGRAM_OFFSET;

    // Clear the flashbuffer
    dma_channel_set_write_addr(dma_chan_1, flashbuffer, true);
    dma_channel_wait_for_finish_blocking(dma_chan_1);

    // Erase the first sector (4096 bytes)
    const uint32_t ints{save_and_disable_interrupts()};
    flash_range_erase(erasure_address, SECTOR_SIZE);
    restore_interrupts(ints);

    // Increment the erasure pointer by 4096 bytes
    erasure_address += SECTOR_SIZE;
    printf("[handleFileStart] - Erased first sector of flash memory, proceeding...\n");
}

// Handles the data in the flashbuffer.
// We read the data inside the main loop, now we need to process it
static inline void handleData() {
    uint32_t ints;
    // Is there space remaining in the erased sector?
    // If so . . .
    if (programming_address < erasure_address) {
        // If this is the first page, copy it to the first_page_buffer
        // and reset first_page
        if (first_page) {
            // Copy contents of flashbuffer to first_page_buffer
            dma_channel_set_read_addr(dma_chan_2, flashbuffer, false);
            dma_channel_set_write_addr(dma_chan_2, first_page_buffer, true);
            dma_channel_wait_for_finish_blocking(dma_chan_2);
            // Reset first_page
            first_page = 0;
        } else {
            // Program flash memory at the programming address
            ints = save_and_disable_interrupts();
            flash_range_program(programming_address, flashbuffer, PAGE_SIZE);
            restore_interrupts(ints);
        }

        // Increment the programming address by page size (256 bytes)
        programming_address += PAGE_SIZE;

        // Clear the flashbuffer. Flashdex autowraps to 0.
        dma_channel_set_write_addr(dma_chan_1, flashbuffer, true);
        dma_channel_wait_for_finish_blocking(dma_chan_1);
    }

    // If not . . .
    else {
        // Erase the next sector (4096 bytes);
        ints = save_and_disable_interrupts();
        flash_range_erase(erasure_address, SECTOR_SIZE);
        restore_interrupts(ints);

        // Increment the erasure pointer by 4096 bytes
        erasure_address += SECTOR_SIZE;

        // Program flash memory at the programming address
        ints = save_and_disable_interrupts();
        flash_range_program(programming_address, flashbuffer, PAGE_SIZE);
        restore_interrupts(ints);

        // Increment the programming address by page size (256 bytes)
        programming_address += PAGE_SIZE;

        // Clear the flashbuffer. Flashdex autowraps to 0.
        dma_channel_set_write_addr(dma_chan_1, flashbuffer, true);
        dma_channel_wait_for_finish_blocking(dma_chan_1);
    }
    gpio_put(LED_PIN, !gpio_get(LED_PIN));
}

// Toggle the LED

// We may have a partially-full buffer when we get the end of file hexline.
// If so, write that last buffer to flash memory.
static inline void writeLastBuffer() {
    printf("[writeLastBuffer] - Writing last buffer to flash memory...\n");
    uint32_t ints;
    // Program the final flashbuffer.
    // Is there space remaining in the erased sector?
    // If so . . .
    if (programming_address < erasure_address) {
        // Program flash memory at the programming address
        ints = save_and_disable_interrupts();
        flash_range_program(programming_address, flashbuffer, PAGE_SIZE);
        restore_interrupts(ints);
    }

    // If not . . .
    else {
        // Erase the next sector (4096 bytes)
        ints = save_and_disable_interrupts();
        flash_range_erase(erasure_address, SECTOR_SIZE);
        // Program flash memory at the programming address
        flash_range_program(programming_address, flashbuffer, PAGE_SIZE);
        restore_interrupts(ints);
    }
}

// We program the first page LAST, since we use it to figure out if there's
// a valid program to vector into.
static inline void writeFirstPage() {
    printf("[writeFirstPage] - Writing first page to flash memory...\n");
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(PROGRAM_OFFSET, first_page_buffer, PAGE_SIZE);
    restore_interrupts(ints);
}

// Before branching to our application, we want to disable interrupts
// and release any resources that we claimed for the bootloader.
static inline void cleanUp() {
    // Disable systick
    // hw_set_bits((io_rw_32 *)0xe000e010, 0xFFFFFFFF);

    // Turn off interrupts (NVIC ICER, NVIC ICPR)
    hw_set_bits((io_rw_32 *) 0xe000e180, 0xFFFFFFFF);
    hw_set_bits((io_rw_32 *) 0xe000e280, 0xFFFFFFFF);

    // SysTick->CTRL &= ~1;

    // Free-up DMA
    dma_channel_cleanup(dma_chan_1);
    dma_channel_unclaim(dma_chan_1);
    dma_channel_cleanup(dma_chan_2);
    dma_channel_unclaim(dma_chan_2);

    // Disable the sniffer
    dma_sniffer_disable();

    deinitCommonResources();
    sd::deinit();
}

// Set VTOR register, set stack pointer, and jump to the reset
// vector in our application. Basically copied from crt0.S.
static inline void handleBranch() {
    // In an assembly snippet . . .
    // Set VTOR register, set stack pointer, and jump to reset
    asm volatile(
        "mov r0, %[start]\n"
        "ldr r1, =%[vtable]\n"
        "str r0, [r1]\n"
        "ldmia r0, {r0, r1}\n"
        "msr msp, r0\n"
        "bx r1\n"
        :
        : [start] "r"(XIP_BASE + PROGRAM_OFFSET), [vtable] "X"(PPB_BASE + M0PLUS_VTOR_OFFSET)
        :);
}

bool validProgram() {
    return (flash_target_contents[0] == 0xF2) &&
           (flash_target_contents[1] == 0xEB) &&
           (flash_target_contents[2] == 0x88) &&
           (flash_target_contents[3] == 0x71);
}

// Main (runs on core 0)
int main() {
    stdio_init_all();
    gpio_init(BOOTLOAD_PIN);
    gpio_pull_up(BOOTLOAD_PIN);

    // Initialize the LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

#ifdef DEBUG_STDIO_DELAY
    sleep_ms(5000);
#endif

    // Did the application send us back to the bootloader?
    // If so, reset WD scratch and go to the bootloader
    if (watchdog_hw->scratch[0]) {
        watchdog_hw->scratch[0] = 0;
    }

    // If not, use the BINARY_INFO_MARKER_START to determine
    // whether there's a valid application to which we can branch.
    else if (validProgram()) {
        // If not, are we supposed to enter the bootloader anyway?
        if (gpio_get(BOOTLOAD_PIN)) {
            printf("[main] - Valid application found, branching to application...\n");
            deinitCommonResources();
            handleBranch();
        } else {
            printf("[main] - BOOTLOAD_PIN is low, forcing bootloader mode...\n");
        }
    }
    // There isn't a valid application, go to the bootloader!

    // Claim a few DMA channels
    dma_chan_1 = dma_claim_unused_channel(true);
    dma_chan_2 = dma_claim_unused_channel(true);

    // Configure the third channel (clears flashbuffer)
    dma_channel_config c1 = dma_channel_get_default_config(dma_chan_1);
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_8);
    channel_config_set_read_increment(&c1, false);
    channel_config_set_write_increment(&c1, true);

    dma_channel_configure(
        dma_chan_1, // Channel to be configured
        &c1, // The configuration we just created
        flashbuffer, // write address
        &zeroval, // read address
        PAGE_SIZE, // Number of transfers; in this case each is 1 byte.
        false // Don't start immediately.
    );

    // Configure the fourth channel (copies flashbuffer to first_page_buffer)
    dma_channel_config c2 = dma_channel_get_default_config(dma_chan_2);
    channel_config_set_transfer_data_size(&c2, DMA_SIZE_8);
    channel_config_set_read_increment(&c2, true);
    channel_config_set_write_increment(&c2, true);

    dma_channel_configure(
        dma_chan_2, // Channel to be configured
        &c2, // The configuration we just created
        first_page_buffer, // write address
        flashbuffer, // read address
        PAGE_SIZE, // Number of transfers; in this case each is 1 byte.
        false // Don't start immediately.
    );

    // Initializing SD card'
    printf("[main] - Initializing SD card...\n");
    sd::init();
    // If we're here, the SD should contain a firmware file SD_FIRMWARE_FILENAME
    // If not, we need to panic and light the LED
    if (!sd::fileExists(SD_FIRMWARE_FILENAME)) {
        gpio_put(LED_PIN, true);
        panic("[main] - Firmware file '%s' not found, execution can't proceed...\n", SD_FIRMWARE_FILENAME);
    } else {
        // Open the file
        sd::openFile(SD_FIRMWARE_FILENAME);
        printf("[main] - Firmware file found, proceeding with loading...\n");
    }

    // Handling first page and starting firmware writing
    first_page = 1;
    handleFileStart();
    int i{0};
    while (true) {
        if (const uint bytes = sd::readPage(flashbuffer); bytes == PAGE_SIZE) {
            handleData();
        } else {
            writeLastBuffer();
            writeFirstPage();
            printf("[main] - Firmware loaded successfully, total bytes written: %dkb\n", i / 4);
            cleanUp();
            handleBranch();
        }
        i++;
        if (i % 0x100 == 0) {
            printf("[main] - %dkb written...\n", i / 4);
        }
    }
}
