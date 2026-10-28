#ifndef _FILEMANAGER_H_
#define _FILEMANAGER_H_

#include "hal/hal.h"
#include "hal/hal_config.h"

class FileManager
{
public:
    void Init();
    void Deinit();
    void Main();

protected:
private:
};

extern FileManager g_fileManager;

#endif
