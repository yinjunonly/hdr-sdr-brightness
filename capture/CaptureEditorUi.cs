using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Windows.Forms;

internal static partial class Program
{
    private const int DwmwaUseImmersiveDarkMode = 20;
    private const int DwmwaUseImmersiveDarkModeLegacy = 19;
    private const int DwmwaBorderColor = 34;
    private const int DwmwaCaptionColor = 35;
    private const int DwmwaTextColor = 36;
    private static readonly nint HwndTopmost = new(-1);
    private static readonly nint HwndNoTopmost = new(-2);
    private const uint SwpNoSize = 0x0001;
    private const uint SwpNoMove = 0x0002;
    private const uint SwpShowWindow = 0x0040;
    private const uint SwpHideWindow = 0x0080;
    private const uint SwpNoActivate = 0x0010;
    private const int SwHide = 0;
    private const int WmHotkey = 0x0312;
    private const int VkEscape = 0x1B;

    private sealed partial class RegionSelectionForm : Form
    {
        private const int EscapeHotkeyId = 6201;
        private Bitmap preview;
        private readonly System.Drawing.Rectangle targetBounds;

    public enum EditMode
    {
        None,
        Marker,
        Ellipse,
        Pen,
        Mosaic
    }

    public enum EditOperationType
    {
        Marker,
        Ellipse,
        Pen,
        Mosaic
    }

    public enum AdjustmentPreset
    {
        Low,
        Balanced,
        High
    }

        private System.Drawing.Point dragStart;
        private System.Drawing.Point dragCurrent;
        private bool dragging;
        private bool selectionReady;
        private EditMode mode = EditMode.None;
        private AdjustmentPreset selectedPreset = AdjustmentPreset.Balanced;
        private int annotationColorIndex;
        private int shapeStrokeWidth = 4;
        private int penStrokeWidth = 6;
        private int mosaicBrushSize = 28;
        private bool editDragging;
        private System.Drawing.Point editStart;
        private System.Drawing.Point editCurrent;
        private readonly List<System.Drawing.Point> currentPenPoints = new();
        private bool currentStrokeInside;
        private readonly List<EditOperation> operations = new();
        private readonly List<EditOperation> redoOperations = new();
        private System.Drawing.Point toolbarTooltipAnchor;
        private bool mouseInputReady;
        private bool closingHidden;
        private readonly nint previousForegroundWindow;

        public System.Drawing.Rectangle SelectedImageRegion { get; private set; }

        public SelectionCommitAction CommitAction { get; private set; } = SelectionCommitAction.Copy;

        public AdjustmentPreset SelectedPreset => selectedPreset;

        public IReadOnlyList<EditOperation> Operations => operations;

        public RegionSelectionForm(Bitmap preview)
        {
            this.preview = preview;
            previousForegroundWindow = GetForegroundWindow();
            targetBounds = Screen.PrimaryScreen?.Bounds ?? new System.Drawing.Rectangle(0, 0, preview.Width, preview.Height);
            Bounds = new System.Drawing.Rectangle(-32000, -32000, targetBounds.Width, targetBounds.Height);
            FormBorderStyle = FormBorderStyle.None;
            StartPosition = FormStartPosition.Manual;
            TopMost = true;
            ShowInTaskbar = false;
            DoubleBuffered = true;
            SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            KeyPreview = true;
            Cursor = Cursors.Cross;
            BackColor = Color.Black;
            mouseInputReady = true;
            Font = new Font(CaptureText.FontFamily, 9.0f, FontStyle.Regular, GraphicsUnit.Point);

            selectionTooltip.Visible = false;
            selectionTooltipTimer.Interval = 1000;
            selectionTooltipTimer.Tick += (_, _) => ShowSelectionTooltip();

            Controls.Add(selectionTooltip);
        }

        protected override void OnHandleCreated(EventArgs e)
        {
            base.OnHandleCreated(e);
            RegisterHotKey(Handle, EscapeHotkeyId, 0, VkEscape);
        }

        protected override void OnHandleDestroyed(EventArgs e)
        {
            UnregisterHotKey(Handle, EscapeHotkeyId);
            base.OnHandleDestroyed(e);
        }

        public void ReplacePreview(Bitmap nextPreview)
        {
            Bitmap oldPreview = preview;
            preview = nextPreview;
            BuildSelectionBackgroundCache();
            Invalidate();
            oldPreview.Dispose();
        }


        private void InvalidateSelectionChange(System.Drawing.Rectangle oldSelection, System.Drawing.Rectangle newSelection)
        {
            System.Drawing.Rectangle dirty = oldSelection.Width <= 0 || oldSelection.Height <= 0
                ? newSelection
                : newSelection.Width <= 0 || newSelection.Height <= 0
                    ? oldSelection
                    : System.Drawing.Rectangle.Union(oldSelection, newSelection);
            dirty = InflateRectangle(dirty, 18);
            dirty.Intersect(ClientRectangle);
            if (dirty.Width > 0 && dirty.Height > 0)
            {
                Invalidate(dirty);
            }
        }


        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            BuildSelectionBackgroundCache();
            Refresh();
            Bounds = targetBounds;
            SetWindowPos(Handle, HwndTopmost, targetBounds.Left, targetBounds.Top,
                targetBounds.Width, targetBounds.Height, SwpShowWindow);
            BuildSelectionBackgroundCache();
            Refresh();
            BringToFront();
            SetForegroundWindow(Handle);
            Activate();
            Focus();
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                selectionTooltipTimer.Dispose();
                selectionBackgroundBitmap?.Dispose();
                preview.Dispose();
            }
            base.Dispose(disposing);
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            HideBeforeClose();
            base.OnFormClosing(e);
        }

        private void HideBeforeClose()
        {
            if (closingHidden || !IsHandleCreated) return;
            closingHidden = true;
            try
            {
                SetWindowPos(Handle, HwndNoTopmost, 0, 0, 0, 0, SwpNoMove | SwpNoSize | SwpNoActivate);
                if (previousForegroundWindow != 0 &&
                    previousForegroundWindow != Handle &&
                    IsWindow(previousForegroundWindow))
                {
                    SetForegroundWindow(previousForegroundWindow);
                }
                ShowWindow(Handle, SwHide);
                SetWindowPos(Handle, HwndTopmost, -32000, -32000,
                    Math.Max(1, Width), Math.Max(1, Height), SwpHideWindow | SwpNoActivate);
                Hide();
            }
            catch
            {
            }
        }



        private void CommitSelection(SelectionCommitAction action)
        {
            if (!selectionReady || SelectedImageRegion.Width <= 0 || SelectedImageRegion.Height <= 0) return;
            CommitAction = action;
            HideBeforeClose();
            DialogResult = DialogResult.OK;
            Close();
        }




    }

    private static void ApplyDarkWindowFrame(nint hwnd)
    {
        if (hwnd == 0) return;

        int enabled = 1;
        _ = DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkMode, ref enabled, sizeof(int));
        _ = DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkModeLegacy, ref enabled, sizeof(int));

        int captionColor = ColorRef(Color.FromArgb(15, 17, 18));
        int borderColor = ColorRef(Color.FromArgb(55, 60, 64));
        int textColor = ColorRef(Color.FromArgb(235, 239, 242));
        _ = DwmSetWindowAttribute(hwnd, DwmwaCaptionColor, ref captionColor, sizeof(int));
        _ = DwmSetWindowAttribute(hwnd, DwmwaBorderColor, ref borderColor, sizeof(int));
        _ = DwmSetWindowAttribute(hwnd, DwmwaTextColor, ref textColor, sizeof(int));
    }

    private static int ColorRef(Color color)
    {
        return color.R | (color.G << 8) | (color.B << 16);
    }

    [DllImport("user32.dll")]
    private static extern nint GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern bool IsWindow(nint hwnd);

    [DllImport("user32.dll")]
    private static extern bool SetWindowDisplayAffinity(nint hwnd, uint affinity);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool RegisterHotKey(nint hwnd, int id, uint fsModifiers, int vk);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnregisterHotKey(nint hwnd, int id);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool SetWindowPos(nint hwnd, nint hwndInsertAfter, int x, int y, int cx, int cy, uint flags);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(nint hwnd, int nCmdShow);

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(nint hwnd);

    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(nint hwnd, int attribute, ref int attributeValue, int attributeSize);

}
