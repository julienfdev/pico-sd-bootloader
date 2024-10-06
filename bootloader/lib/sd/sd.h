#ifndef SD_H
#define SD_H
namespace sd
{
    void init();
    bool fileExists(const char *filename);
    bool openFile(const char *filename);
    void deinit();
    unsigned int readPage(unsigned char *buffer);
}
#endif
