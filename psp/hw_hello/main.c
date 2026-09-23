

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>

PSP_MODULE_INFO("FH Hello", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(64);

int main(void) {
    pspDebugScreenInit();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("HELLO PSP\n");
    pspDebugScreenPrintf("If you see this, homebrew boots.\n");
    sceDisplayWaitVblankStart();
    sceKernelDelayThread(4000000);
    sceKernelExitGame();
    return 0;
}
