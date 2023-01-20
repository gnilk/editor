//
// Created by gnilk on 19.01.23.
//
#include "Core/unix/Shell.h"

int main(int argc, char **argv) {
    Shell shell;
    static int lc = 0;
    shell.SetStdoutDelegate([](std::string &str) {
        // FIXME: needs to trim (should perhaps be done in the consumer)
        printf("%d: %s\n", lc, str.c_str());
        lc++;
    });

    shell.Begin();
    char buffer[256];
    while(true) {
        auto res = fgets(buffer, 256, stdin);
        if (res == nullptr) {
            continue;
        }
        std::string cmdString(buffer);
        shell.SendCmd(cmdString);
    }
    shell.Close();

}