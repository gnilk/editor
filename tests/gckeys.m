//
// Created by gnilk on 13.01.23.
//
#import <GameController/GCKeyboard.h>
#import <GameController/GCController.h>
int main(int argc, char **argv) {
    int count = 0;
    for(GCController *obj in GCController.controllers) {
        count++;
        const char *stringAsChar = [obj.vendorName cStringUsingEncoding:[NSString defaultCStringEncoding]];
        printf("%s\n",stringAsChar);
    }
    printf("Count: %d\n", count);
}