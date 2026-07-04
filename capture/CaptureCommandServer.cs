using System.Diagnostics;
using System.IO.Pipes;
using System.Text;
using Windows.Graphics.DirectX;

internal static partial class Program
{
    private const string PipeName = "HdrSdrBrightnessCapture";
    private static int activeRegionCapture;

    private static async Task<int> RunCommandServerAsync(string[] args)
    {
        using CancellationTokenSource cancellation = new();
        int parentPid = CaptureArgs.Int(args, "--parent-pid", 0);
        int requestedIdleTimeoutMs = CaptureArgs.Int(args, "--idle-timeout-ms", 90000);
        int idleTimeoutMs = requestedIdleTimeoutMs <= 0 ? 0 : Math.Max(5000, requestedIdleTimeoutMs);
        if (parentPid > 0)
        {
            _ = Task.Run(async () =>
            {
                try
                {
                    using Process parent = Process.GetProcessById(parentPid);
                    while (!parent.HasExited && !cancellation.IsCancellationRequested)
                    {
                        await Task.Delay(1000, cancellation.Token);
                        parent.Refresh();
                    }
                }
                catch
                {
                }

                cancellation.Cancel();
            });
        }

        _ = WgcCapture.GetSharedRuntimeAsync();

        while (!cancellation.IsCancellationRequested)
        {
            try
            {
                await using NamedPipeServerStream pipe = new(
                    PipeName,
                    PipeDirection.InOut,
                    1,
                    PipeTransmissionMode.Byte,
                    PipeOptions.Asynchronous);
                Task waitForConnection = pipe.WaitForConnectionAsync(cancellation.Token);
                Task completed = idleTimeoutMs > 0
                    ? await Task.WhenAny(waitForConnection, Task.Delay(idleTimeoutMs, cancellation.Token))
                    : await Task.WhenAny(waitForConnection);
                if (completed != waitForConnection)
                {
                    if (System.Threading.Volatile.Read(ref activeRegionCapture) == 1)
                    {
                        continue;
                    }
                    break;
                }
                await waitForConnection;
                using StreamReader reader = new(pipe, Encoding.Unicode, false, 1024, leaveOpen: true);
                string? command = await reader.ReadLineAsync(cancellation.Token);
                if (!string.IsNullOrWhiteSpace(command))
                {
                    int resultCode = await DispatchServerCommandAsync(command, cancellation.Token);
                    await TryWritePipeResponseAsync(pipe, resultCode, cancellation.Token);
                }
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (IOException)
            {
            }
            catch (Exception ex)
            {
                Debug.WriteLine(ex);
                await Task.Delay(250);
            }
        }

        return 0;
    }

    private static async Task<int> DispatchServerCommandAsync(string command, CancellationToken cancellationToken)
    {
        string[] parts = command.Split('\t');
        if (parts.Length == 0) return 2;

        if (string.Equals(parts[0], "select-region", StringComparison.OrdinalIgnoreCase))
        {
            if (System.Threading.Interlocked.Exchange(ref activeRegionCapture, 1) == 1)
            {
                return 2;
            }

            int language = ParseCommandInt(parts, 1, 0);
            _ = Task.Run(async () =>
            {
                try
                {
                    await RunServerRegionCaptureAsync(language);
                }
                catch (Exception ex)
                {
                    Debug.WriteLine(ex);
                }
                finally
                {
                    System.Threading.Volatile.Write(ref activeRegionCapture, 0);
                }
            }, cancellationToken);
            return 0;
        }

        return await HandleServerCommandAsync(parts);
    }

    private static int ParseCommandInt(string[] parts, int index, int fallback)
    {
        return index < parts.Length &&
            int.TryParse(parts[index], System.Globalization.NumberStyles.Integer,
                System.Globalization.CultureInfo.InvariantCulture, out int value)
            ? value
            : fallback;
    }

    private static async Task TryWritePipeResponseAsync(NamedPipeServerStream pipe, int resultCode, CancellationToken cancellationToken)
    {
        if (!pipe.IsConnected || !pipe.CanWrite) return;
        try
        {
            await using StreamWriter writer = new(pipe, Encoding.Unicode, 1024, leaveOpen: true);
            await writer.WriteLineAsync(resultCode.ToString(System.Globalization.CultureInfo.InvariantCulture).AsMemory(), cancellationToken);
            await writer.FlushAsync(cancellationToken);
        }
        catch
        {
        }
    }

    private static async Task RunServerRegionCaptureAsync(int language)
    {
        if (language > 0) CaptureText.SetLanguageId(language);

        ToneMapOptions toneMap = ToneMapOptions.FromArgs(Array.Empty<string>());
        string outputPath = ResolveOutputPath(Array.Empty<string>());
        await CaptureSelectedRegionFastAsync(DirectXPixelFormat.R16G16B16A16Float, outputPath, toneMap, false);
    }

    private static async Task<int> HandleServerCommandAsync(string[] parts)
    {
        if (string.Equals(parts[0], "fullscreen-clip", StringComparison.OrdinalIgnoreCase))
        {
            int language = ParseCommandInt(parts, 1, 0);
            if (language > 0) CaptureText.SetLanguageId(language);

            string outputPath = parts.Length > 2 && !string.IsNullOrWhiteSpace(parts[2])
                ? parts[2]
                : ResolveOutputPath(Array.Empty<string>());
            ToneMapOptions toneMap = ToneMapOptions.FromArgs(Array.Empty<string>());
            try
            {
                CaptureRuntime runtime = await WgcCapture.GetSharedRuntimeAsync();
                ReadbackResult result = await WgcCapture.CaptureFrameAsync(runtime.Item, runtime.Device, runtime.Native,
                    DirectXPixelFormat.R16G16B16A16Float, toneMap, false);
                RunSta(() => CopyBgraToClipboard(result.Width, result.Height, result.Bgra));
                if (!string.IsNullOrWhiteSpace(outputPath))
                {
                    await SaveFullscreenEditImageAsync(outputPath, result.Width, result.Height, result.Bgra);
                }
                return 0;
            }
            catch (TimeoutException)
            {
                Console.WriteLine("Timed out waiting for a WGC frame.");
                WgcCapture.ResetSharedRuntime();
                return 4;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Capture failed: {ex.GetType().Name}: {ex.Message}");
                WgcCapture.ResetSharedRuntime();
                return 8;
            }
        }

        return 2;
    }

}
