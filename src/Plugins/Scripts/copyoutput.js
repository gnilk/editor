// '.co' - copy the output of the last command run in the terminal (a build's errors, say) to the clipboard
function main(args) {
    if (!Terminal.CopyLastOutputToClipboard()) {
        Console.WriteLine("No command output to copy");
    }
}
