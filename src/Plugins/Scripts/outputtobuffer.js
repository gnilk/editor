// '.ob' - open the output of the last command run in the terminal in a new buffer
function main(args) {
    if (!Terminal.OpenLastOutputAsDocument()) {
        Console.WriteLine("No command output to open");
    }
}
