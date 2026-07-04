using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;

internal static partial class Program
{
    private static string ResolveOutputPath(string[] args)
    {
        string? requested = CaptureArgs.Value(args, "--output");
        if (!string.IsNullOrWhiteSpace(requested))
        {
            return Path.GetFullPath(Environment.ExpandEnvironmentVariables(requested));
        }

        string pictures = Environment.GetFolderPath(Environment.SpecialFolder.MyPictures);
        if (string.IsNullOrWhiteSpace(pictures)) pictures = Environment.CurrentDirectory;
        string directory = Path.Combine(pictures, "HDR SDR Brightness");
        string fileName = $"HDR-SDR-Capture-{DateTime.Now:yyyyMMdd-HHmmss}.png";
        return Path.Combine(directory, fileName);
    }

    private static void OpenOutputInExplorer(string outputPath)
    {
        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = "explorer.exe",
                Arguments = $"/select,\"{outputPath}\"",
                UseShellExecute = false
            });
        }
        catch
        {
        }
    }

    private static Bitmap CreateFastScreenPreviewBitmap()
    {
        System.Drawing.Rectangle bounds = Screen.PrimaryScreen?.Bounds ??
            new System.Drawing.Rectangle(0, 0, 1, 1);
        if (bounds.Width <= 0 || bounds.Height <= 0)
        {
            bounds = new System.Drawing.Rectangle(0, 0, 1, 1);
        }

        Bitmap bitmap = new(bounds.Width, bounds.Height, PixelFormat.Format32bppArgb);
        bitmap.SetResolution(96.0f, 96.0f);
        using Graphics graphics = Graphics.FromImage(bitmap);
        graphics.CopyFromScreen(bounds.Left, bounds.Top, 0, 0, bounds.Size, CopyPixelOperation.SourceCopy);
        return bitmap;
    }

    private static string? PromptSaveAsPath(string defaultOutputPath)
    {
        string? selectedPath = null;
        RunSta(() =>
        {
            string? directory = Path.GetDirectoryName(defaultOutputPath);
            if (!string.IsNullOrWhiteSpace(directory))
            {
                Directory.CreateDirectory(directory);
            }

            using SaveFileDialog dialog = new()
            {
                Title = CaptureText.Get(CaptureString.SaveDialogTitle),
                Filter = CaptureText.Get(CaptureString.PngFilter),
                DefaultExt = "png",
                AddExtension = true,
                OverwritePrompt = true,
                FileName = Path.GetFileName(defaultOutputPath),
                InitialDirectory = directory
            };
            selectedPath = dialog.ShowDialog() == DialogResult.OK ? dialog.FileName : null;
        });
        return selectedPath;
    }

    private static async Task SavePngAsync(string path, uint width, uint height, byte[] bgra)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using Bitmap bitmap = CreateBitmapFromBgra(width, height, bgra);
        await Task.Run(() => bitmap.Save(path, ImageFormat.Png));
    }

    private static Task SaveFullscreenEditImageAsync(string path, uint width, uint height, byte[] bgra)
    {
        if (!string.Equals(Path.GetExtension(path), ".bmp", StringComparison.OrdinalIgnoreCase))
        {
            return SavePngAsync(path, width, height, bgra);
        }

        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        using Bitmap bitmap = CreateBitmapFromBgra(width, height, bgra);
        bitmap.Save(path, ImageFormat.Bmp);
        return Task.CompletedTask;
    }

    private static void ShowPreviewEditor(ReadbackResult result, string defaultOutputPath)
    {
        using Bitmap bitmap = CreateBitmapFromBgra(result.Width, result.Height, result.Bgra);
        ShowPreviewEditor(bitmap, defaultOutputPath);
    }

    private static void ShowPreviewEditor(Bitmap source, string defaultOutputPath, bool copyOnShown = true)
    {
        RunSta(() =>
        {
            using RegionSelectionForm.PreviewEditorForm editor = new(new Bitmap(source), defaultOutputPath, copyOnShown);
            editor.ShowDialog();
        });
    }

    private static int EditExistingImage(string path, string defaultOutputPath, bool copyOnShown)
    {
        try
        {
            string fullPath = Path.GetFullPath(Environment.ExpandEnvironmentVariables(path));
            if (!File.Exists(fullPath))
            {
                Console.WriteLine($"Image not found: {fullPath}");
                return 2;
            }

            using Image loaded = Image.FromFile(fullPath);
            using Bitmap bitmap = new(loaded);
            bitmap.SetResolution(96.0f, 96.0f);
            ShowPreviewEditor(bitmap, defaultOutputPath, copyOnShown);
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Edit failed: {ex.GetType().Name}: {ex.Message}");
            return 8;
        }
    }

    private static void CopyBgraToClipboard(uint width, uint height, byte[] bgra)
    {
        using Bitmap bitmap = CreateBitmapFromBgra(width, height, bgra);
        RunSta(() =>
        {
            using Bitmap clipboardBitmap = new(bitmap);
            clipboardBitmap.SetResolution(96.0f, 96.0f);
            using MemoryStream pngStream = new();
            clipboardBitmap.Save(pngStream, ImageFormat.Png);
            DataObject data = new();
            data.SetData("PNG", false, new MemoryStream(pngStream.ToArray()));
            data.SetData(DataFormats.Bitmap, true, new Bitmap(clipboardBitmap));
            Clipboard.SetDataObject(data, true, 5, 120);
        });
    }
}
