using System;
using System.Diagnostics;
using System.IO;
using System.Text;

// Task Scheduler entry point. No user interface and no arbitrary command input.
static class NoConsoleRunner {
    static int Main() {
        string root=AppDomain.CurrentDomain.BaseDirectory;
        try {
            string previousError=Path.Combine(root,"host-launcher.error.log");
            if(File.Exists(previousError)) File.Delete(previousError);
            string script=Path.Combine(root,"HyperVRunnerTask.ps1");
            if(!File.Exists(script)) throw new FileNotFoundException("Protected controller script is missing.",script);
            using(var process=new Process()) {
                process.StartInfo.FileName=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell\\v1.0\\powershell.exe");
                process.StartInfo.Arguments="-NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""+script+"\"";
                process.StartInfo.WorkingDirectory=root;
                process.StartInfo.UseShellExecute=false;
                process.StartInfo.CreateNoWindow=true;
                process.StartInfo.RedirectStandardOutput=true;
                process.StartInfo.RedirectStandardError=true;
                var encoding=Encoding.GetEncoding(System.Globalization.CultureInfo.CurrentCulture.TextInfo.OEMCodePage);
                process.StartInfo.StandardOutputEncoding=encoding;
                process.StartInfo.StandardErrorEncoding=encoding;
                if(!process.Start()) throw new InvalidOperationException("Cannot start the protected controller.");
                var output=process.StandardOutput.ReadToEndAsync();
                var errors=process.StandardError.ReadToEndAsync();
                process.WaitForExit();
                if(!output.Wait(5000) || !errors.Wait(5000)) throw new TimeoutException("Controller output streams did not close.");
                File.WriteAllText(Path.Combine(root,"host-controller.stdout.log"),output.Result,new UTF8Encoding(false));
                File.WriteAllText(Path.Combine(root,"host-controller.stderr.log"),errors.Result,new UTF8Encoding(false));
                return process.ExitCode;
            }
        } catch(Exception error) {
            try { File.WriteAllText(Path.Combine(root,"host-launcher.error.log"),error.ToString()); }
            catch { /* Preserve the failing exit even when a log cannot be written. */ }
            return 1;
        }
    }
}
