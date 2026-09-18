# Run each compiler/test process with a wall-clock limit and capture its output.
function Run-Checked([string]$Program, [string[]]$Arguments, [string]$InputText, [int]$ExpectedExit = 0) {
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = if (Test-Path -LiteralPath $Program) { (Resolve-Path -LiteralPath $Program).Path } else { $Program }
    $info.WorkingDirectory = (Get-Location).Path
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.RedirectStandardInput = $true
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $info
    try {
        if (!$process.Start()) { throw "Could not start $Program" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if ($InputText) { $process.StandardInput.Write($InputText) }
        $process.StandardInput.Close()
        if (!$process.WaitForExit(30000)) {
            $process.Kill($true)
            throw "$Program exceeded the 30-second limit and was stopped."
        }
        $output = $stdout.GetAwaiter().GetResult()
        $errors = $stderr.GetAwaiter().GetResult()
        if ($output) { Write-Output $output.TrimEnd() }
        if ($errors) { Write-Host $errors.TrimEnd() }
        if ($process.ExitCode -ne $ExpectedExit) { throw "$Program failed: $($process.ExitCode)" }
    } finally { $process.Dispose() }
}


