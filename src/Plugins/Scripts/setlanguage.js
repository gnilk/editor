function main(args) {
    var textBuffer = Editor.GetActiveTextBuffer();
    if (textBuffer != null) {
        //Logger.Debug("Setting new language to active buffer");
        textBuffer.SetLanguage(args[0]);
        delete textBuffer;
    }
}