function main(args) {
    var helpList = Editor.GetCommandDescriptions();
    for(var i = 0; i<helpList.length;i++) {
        var str = helpList[i];
        Console.WriteLine(str);
    }
}