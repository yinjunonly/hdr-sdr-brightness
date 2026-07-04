using System.Windows.Forms;
using Windows.Graphics.Capture;
using WinRT.Interop;

internal static partial class Program
{
    private static Form CreateHiddenOwnerWindow()
    {
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Form owner = new()
        {
            Text = CaptureText.Get(CaptureString.HiddenOwnerTitle),
            ShowInTaskbar = false,
            StartPosition = FormStartPosition.Manual,
            Size = new System.Drawing.Size(1, 1),
            Location = new System.Drawing.Point(-32000, -32000)
        };
        owner.Show();
        return owner;
    }

    private static async Task<GraphicsCaptureItem?> PickCaptureItemAsync(nint ownerHwnd)
    {
        GraphicsCapturePicker picker = new();
        InitializeWithWindow.Initialize(picker, ownerHwnd);
        return await picker.PickSingleItemAsync();
    }

}
