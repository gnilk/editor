function main(args) {
    var buffers = Editor.GetBuffers();
    Console.WriteLine("Buffers: ", buffers.length);
    for(var i=0;i<buffers.length;i++) {
        var buf = buffers[i];
        var name = buf.GetName();
        Console.WriteLine(i, "  - ", name);
    }
    Console.WriteLine("-- Footer --");
}