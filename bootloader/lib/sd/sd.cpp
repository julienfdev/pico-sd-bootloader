#include "sd.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hw_config.h"

FATFS fs;
FIL file;

#ifndef SD_PAGE_SIZE
#define SD_PAGE_SIZE 256
#endif



namespace sd
{
    void init()
    {
        // Mount the default drive
        if (FRESULT const fr{f_mount(&fs, "", 1)}; fr != FR_OK)
        {
            panic("[sd::init] - Failed to mount drive. Error: %d\n", fr);
        }
        else
        {
            printf("[sd::init] - Drive mounted successfully.\n");
        }
    }

    bool fileExists(const char *filename)
    {
        return f_stat(filename, nullptr) == FR_OK;
    }

    bool openFile(const char *filename)
    {
        return f_open(&file, filename, FA_READ) == FR_OK;
    }

    void deinit()
    {
        // Close the file
        f_close(&file);

        // Unmount the drive
        f_mount(nullptr, "", 0);
    }

    unsigned int readPage(unsigned char *buffer)
    {
        uint8_t *tmp_buffer[SD_PAGE_SIZE]{nullptr};
        uint br;

        // Read a page from the file
        f_read(&file, tmp_buffer, SD_PAGE_SIZE, &br);

        // if there is less bytes read than the page size, fill the rest with nullptr
        if (br < SD_PAGE_SIZE)
        {
            for (uint i = br; i < SD_PAGE_SIZE; i++)
            {
                tmp_buffer[i] = nullptr;
            }
        }

        // copy the temp buffer to the buffer
        memcpy(buffer, tmp_buffer, SD_PAGE_SIZE);

        // return the number of bytes actually read
        return br;
    }
}
